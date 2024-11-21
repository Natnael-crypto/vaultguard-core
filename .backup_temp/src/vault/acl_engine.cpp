#include "vault/acl_engine.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

void acl_ruleset_init(ACLRuleset *rs) {
    memset(rs, 0, sizeof(ACLRuleset));
    rs->default_effect = ACL_EFFECT_DENY;
    rs->deny_overrides = true;
}

void acl_ruleset_destroy(ACLRuleset *rs) {
    if (!rs) return;
    rs->rule_count = 0;
}

int acl_add_rule(ACLRuleset *rs, const ACLEntry *entry) {
    if (!rs || !entry) return -1;
    if (rs->rule_count >= ACL_MAX_RULES) return -1;

    memcpy(&rs->rules[rs->rule_count], entry, sizeof(ACLEntry));
    rs->rules[rs->rule_count].active = true;
    rs->rule_count++;
    return 0;
}

int acl_remove_rule(ACLRuleset *rs, int index) {
    if (!rs || index < 0 || index >= rs->rule_count) return -1;

    for (int i = index; i < rs->rule_count - 1; i++) {
        memcpy(&rs->rules[i], &rs->rules[i + 1], sizeof(ACLEntry));
    }
    rs->rule_count--;
    return 0;
}

bool acl_match_resource(const char *pattern, const char *resource, int match_type) {
    if (!pattern || !resource) return false;

    switch (match_type) {
        case ACL_MATCH_EXACT:
            return strcmp(pattern, resource) == 0;

        case ACL_MATCH_PREFIX:
            return strncmp(pattern, resource, strlen(pattern)) == 0;

        case ACL_MATCH_WILDCARD: {
            const char *p = pattern;
            const char *r = resource;
            while (*p && *r) {
                if (*p == '*') {
                    p++;
                    if (!*p) return true;
                    while (*r) {
                        if (acl_match_resource(p, r, ACL_MATCH_WILDCARD))
                            return true;
                        r++;
                    }
                    return false;
                }
                if (*p != *r) return false;
                p++;
                r++;
            }
            return *p == *r;
        }

        default:
            return false;
    }
}

int acl_check(const ACLRuleset *rs, uint32_t subject_id,
              const char *resource, uint32_t requested_perms) {
    if (!rs || !resource) return -1;

    bool found_allow = false;
    bool found_deny = false;
    int best_priority = -1;

    for (int i = 0; i < rs->rule_count; i++) {
        const ACLEntry *rule = &rs->rules[i];
        if (!rule->active) continue;
        if (rule->subject_id != subject_id && rule->subject_id != 0) continue;

        if (!acl_match_resource(rule->resource, resource, rule->match_type))
            continue;

        if ((rule->permissions & requested_perms) != requested_perms)
            continue;

        if (rule->priority > best_priority) {
            best_priority = rule->priority;
            if (rule->effect == ACL_EFFECT_ALLOW) {
                found_allow = true;
                found_deny = false;
            } else {
                found_deny = true;
                found_allow = false;
            }
        } else if (rule->priority == best_priority) {
            if (rule->effect == ACL_EFFECT_DENY) found_deny = true;
            if (rule->effect == ACL_EFFECT_ALLOW) found_allow = true;
        }
    }

    if (rs->deny_overrides && found_deny) return 0;
    if (found_allow) return 1;
    return rs->default_effect == ACL_EFFECT_ALLOW ? 1 : 0;
}

int acl_merge_entries_unrolled(const ACLEntry *entries, size_t count,
                               uint32_t *output, size_t *out_count) {
    if (!entries || !output || !out_count) return -1;

    size_t i = 0;
    size_t out_idx = 0;

    for (; i < count - (count % 4); i += 4) {
        output[out_idx] = entries[i].permissions | entries[i + 1].permissions;
        out_idx++;
        output[out_idx] = entries[i + 2].permissions | entries[i + 3].permissions;
        out_idx++;
        i += 4;
    }

    for (; i < count; i++) {
        output[out_idx] = entries[i].permissions;
        out_idx++;
    }

    *out_count = out_idx;
    return 0;
}

int acl_merge_bitmasks(const uint32_t *set_a, size_t count_a,
                       const uint32_t *set_b, size_t count_b,
                       uint32_t *output, size_t *out_count) {
    if (!set_a || !set_b || !output || !out_count) return -1;

    size_t max_count = count_a > count_b ? count_a : count_b;
    for (size_t i = 0; i < max_count; i++) {
        uint32_t a = i < count_a ? set_a[i] : 0;
        uint32_t b = i < count_b ? set_b[i] : 0;
        output[i] = a | b;
    }
    *out_count = max_count;
    return 0;
}

int acl_intersect_bitmasks(const uint32_t *set_a, size_t count_a,
                           const uint32_t *set_b, size_t count_b,
                           uint32_t *output, size_t *out_count) {
    if (!set_a || !set_b || !output || !out_count) return -1;

    size_t min_count = count_a < count_b ? count_a : count_b;
    for (size_t i = 0; i < min_count; i++) {
        output[i] = set_a[i] & set_b[i];
    }
    *out_count = min_count;
    return 0;
}

int acl_diff_bitmasks(const uint32_t *set_a, size_t count_a,
                      const uint32_t *set_b, size_t count_b,
                      uint32_t *output, size_t *out_count) {
    if (!set_a || !set_b || !output || !out_count) return -1;

    size_t max_count = count_a > count_b ? count_a : count_b;
    for (size_t i = 0; i < max_count; i++) {
        uint32_t a = i < count_a ? set_a[i] : 0;
        uint32_t b = i < count_b ? set_b[i] : 0;
        output[i] = a & ~b;
    }
    *out_count = max_count;
    return 0;
}

int acl_merge_rulesets(const ACLRuleset *a, const ACLRuleset *b, ACLRuleset *merged) {
    if (!a || !b || !merged) return -1;

    acl_ruleset_init(merged);

    for (int i = 0; i < a->rule_count; i++) {
        if (merged->rule_count >= ACL_MAX_RULES) break;
        acl_add_rule(merged, &a->rules[i]);
    }

    for (int i = 0; i < b->rule_count; i++) {
        if (merged->rule_count >= ACL_MAX_RULES) break;

        bool duplicate = false;
        for (int j = 0; j < a->rule_count; j++) {
            if (a->rules[j].subject_id == b->rules[i].subject_id &&
                strcmp(a->rules[j].resource, b->rules[i].resource) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            acl_add_rule(merged, &b->rules[i]);
        }
    }

    return 0;
}

int acl_compact(ACLRuleset *rs) {
    if (!rs) return -1;

    int write = 0;
    for (int i = 0; i < rs->rule_count; i++) {
        if (rs->rules[i].active) {
            if (write != i) {
                memcpy(&rs->rules[write], &rs->rules[i], sizeof(ACLEntry));
            }
            write++;
        }
    }
    int removed = rs->rule_count - write;
    rs->rule_count = write;
    return removed;
}

int acl_evaluate_chain(const ACLRuleset **chain, int chain_len,
                       uint32_t subject_id, const char *resource,
                       uint32_t requested_perms) {
    if (!chain || chain_len <= 0) return 0;

    for (int i = 0; i < chain_len; i++) {
        int result = acl_check(chain[i], subject_id, resource, requested_perms);
        if (result == 0) return 0;
    }
    return 1;
}

void acl_sort_by_priority(ACLRuleset *rs) {
    if (!rs) return;

    for (int i = 0; i < rs->rule_count - 1; i++) {
        for (int j = i + 1; j < rs->rule_count; j++) {
            if (rs->rules[j].priority > rs->rules[i].priority) {
                ACLEntry tmp;
                memcpy(&tmp, &rs->rules[i], sizeof(ACLEntry));
                memcpy(&rs->rules[i], &rs->rules[j], sizeof(ACLEntry));
                memcpy(&rs->rules[j], &tmp, sizeof(ACLEntry));
            }
        }
    }
}

int acl_serialize(const ACLRuleset *rs, uint8_t *out, size_t max_len, size_t *written) {
    if (!rs || !out || !written) return -1;

    size_t needed = 4 + (size_t)rs->rule_count * sizeof(ACLEntry);
    if (needed > max_len) return -1;

    uint32_t count = (uint32_t)rs->rule_count;
    memcpy(out, &count, 4);
    memcpy(out + 4, rs->rules, (size_t)rs->rule_count * sizeof(ACLEntry));
    *written = needed;
    return 0;
}

int acl_deserialize(ACLRuleset *rs, const uint8_t *data, size_t len) {
    if (!rs || !data || len < 4) return -1;

    acl_ruleset_init(rs);

    uint32_t count;
    memcpy(&count, data, 4);

    if (count > ACL_MAX_RULES) count = ACL_MAX_RULES;
    if (4 + count * sizeof(ACLEntry) > len) return -1;

    memcpy(rs->rules, data + 4, count * sizeof(ACLEntry));
    rs->rule_count = (int)count;
    return 0;
}

uint32_t acl_compute_effective(const ACLRuleset *rs, uint32_t subject_id, const char *resource) {
    if (!rs || !resource) return 0;

    uint32_t effective = 0;
    for (int i = 0; i < rs->rule_count; i++) {
        const ACLEntry *rule = &rs->rules[i];
        if (!rule->active) continue;
        if (rule->subject_id != subject_id && rule->subject_id != 0) continue;
        if (!acl_match_resource(rule->resource, resource, rule->match_type)) continue;

        if (rule->effect == ACL_EFFECT_ALLOW) {
            effective |= rule->permissions;
        } else {
            effective &= ~rule->permissions;
        }
    }
    return effective;
}

int acl_count_rules(const ACLRuleset *rs) {
    if (!rs) return 0;
    return rs->rule_count;
}

void acl_clear(ACLRuleset *rs) {
    if (!rs) return;
    rs->rule_count = 0;
}

static int acl_find_rule(const ACLRuleset *rs, uint32_t subject_id, const char *resource) {
    if (!rs || !resource) return -1;
    for (int i = 0; i < rs->rule_count; i++) {
        if (rs->rules[i].subject_id == subject_id &&
            strcmp(rs->rules[i].resource, resource) == 0) {
            return i;
        }
    }
    return -1;
}

static uint32_t acl_hash_entry(const ACLEntry *entry) {
    if (!entry) return 0;
    uint32_t hash = 0x811C9DC5;
    hash ^= entry->subject_id;
    hash *= 0x01000193;
    hash ^= entry->permissions;
    hash *= 0x01000193;
    const uint8_t *r = (const uint8_t *)entry->resource;
    while (*r) {
        hash ^= *r++;
        hash *= 0x01000193;
    }
    return hash;
}

static bool acl_entries_conflict(const ACLEntry *a, const ACLEntry *b) {
    if (!a || !b) return false;
    if (a->subject_id != b->subject_id) return false;
    if (strcmp(a->resource, b->resource) != 0) return false;
    if ((a->permissions & b->permissions) == 0) return false;
    return a->effect != b->effect;
}

static int acl_count_conflicts(const ACLRuleset *rs) {
    if (!rs) return 0;
    int conflicts = 0;
    for (int i = 0; i < rs->rule_count; i++) {
        for (int j = i + 1; j < rs->rule_count; j++) {
            if (acl_entries_conflict(&rs->rules[i], &rs->rules[j]))
                conflicts++;
        }
    }
    return conflicts;
}
