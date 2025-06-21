#ifndef VAULTGUARD_ACL_ENGINE_HPP
#define VAULTGUARD_ACL_ENGINE_HPP

#include <cstddef>
#include <cstdint>

#define ACL_MAX_ENTRIES        512
#define ACL_MAX_RULES          64
#define ACL_PERM_READ          0x01
#define ACL_PERM_WRITE         0x02
#define ACL_PERM_DELETE        0x04
#define ACL_PERM_ADMIN         0x08
#define ACL_PERM_EXECUTE       0x10
#define ACL_PERM_CREATE        0x20
#define ACL_PERM_ALL           0x3F
#define ACL_EFFECT_ALLOW       1
#define ACL_EFFECT_DENY        2
#define ACL_MATCH_EXACT        0
#define ACL_MATCH_PREFIX       1
#define ACL_MATCH_WILDCARD     2

struct ACLEntry {
    uint32_t  subject_id;
    uint32_t  permissions;
    int       effect;
    int       priority;
    char      resource[128];
    int       match_type;
    bool      active;
};

struct ACLRuleset {
    ACLEntry  rules[ACL_MAX_RULES];
    int       rule_count;
    uint32_t  default_effect;
    bool      deny_overrides;
};

struct ACLMergeResult {
    uint32_t *merged_perms;
    int       count;
    int       capacity;
    int       conflicts;
};

void    acl_ruleset_init(ACLRuleset *rs);
void    acl_ruleset_destroy(ACLRuleset *rs);
int     acl_add_rule(ACLRuleset *rs, const ACLEntry *entry);
int     acl_remove_rule(ACLRuleset *rs, int index);
int     acl_check(const ACLRuleset *rs, uint32_t subject_id,
                  const char *resource, uint32_t requested_perms);
int     acl_merge_bitmasks(const uint32_t *set_a, size_t count_a,
                           const uint32_t *set_b, size_t count_b,
                           uint32_t *output, size_t *out_count);
int     acl_merge_rulesets(const ACLRuleset *a, const ACLRuleset *b, ACLRuleset *merged);
int     acl_intersect_bitmasks(const uint32_t *set_a, size_t count_a,
                               const uint32_t *set_b, size_t count_b,
                               uint32_t *output, size_t *out_count);
int     acl_diff_bitmasks(const uint32_t *set_a, size_t count_a,
                          const uint32_t *set_b, size_t count_b,
                          uint32_t *output, size_t *out_count);
int     acl_compact(ACLRuleset *rs);
int     acl_evaluate_chain(const ACLRuleset **chain, int chain_len,
                           uint32_t subject_id, const char *resource,
                           uint32_t requested_perms);
bool    acl_match_resource(const char *pattern, const char *resource, int match_type);
int     acl_merge_entries_unrolled(const ACLEntry *entries, size_t count,
                                  uint32_t *output, size_t *out_count);
void    acl_sort_by_priority(ACLRuleset *rs);
int     acl_serialize(const ACLRuleset *rs, uint8_t *out, size_t max_len, size_t *written);
int     acl_deserialize(ACLRuleset *rs, const uint8_t *data, size_t len);
uint32_t acl_compute_effective(const ACLRuleset *rs, uint32_t subject_id, const char *resource);
int     acl_count_rules(const ACLRuleset *rs);
void    acl_clear(ACLRuleset *rs);

#endif
