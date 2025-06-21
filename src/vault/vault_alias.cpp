#include "vault/vault_alias.hpp"
#include <cstdlib>
#include <cstring>

void alias_registry_init(AliasRegistry *reg) {
    memset(reg, 0, sizeof(AliasRegistry));
    reg->next_id = 1;
    reg->initialized = true;
}

void alias_registry_destroy(AliasRegistry *reg) {
    if (!reg) return;
    reg->count = 0;
    reg->initialized = false;
}

AliasEntry *alias_find(AliasRegistry *reg, const char *alias_name) {
    if (!reg || !alias_name) return nullptr;
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].active &&
            strcmp(reg->entries[i].alias_name, alias_name) == 0) {
            return &reg->entries[i];
        }
    }
    return nullptr;
}

int alias_create(AliasRegistry *reg, const char *alias_name,
                 const char *target_path, VaultNode *target) {
    if (!reg || !alias_name || !target) return -1;
    if (reg->count >= ALIAS_MAX_ENTRIES) return -1;
    if (alias_find(reg, alias_name)) return -1;

    AliasEntry *entry = &reg->entries[reg->count];
    memset(entry, 0, sizeof(AliasEntry));

    strncpy(entry->alias_name, alias_name, ALIAS_NAME_MAX - 1);
    if (target_path)
        strncpy(entry->target_path, target_path, sizeof(entry->target_path) - 1);

    entry->target = target;
    entry->alias_id = reg->next_id++;
    entry->active = true;
    entry->resolve_count = 0;

    reg->count++;
    return 0;
}

int alias_remove(AliasRegistry *reg, const char *alias_name) {
    if (!reg || !alias_name) return -1;

    AliasEntry *entry = alias_find(reg, alias_name);
    if (!entry) return -1;

    entry->active = false;
    entry->target = nullptr;
    return 0;
}

VaultNode *alias_resolve(AliasRegistry *reg, const char *alias_name) {
    if (!reg || !alias_name) return nullptr;

    AliasEntry *entry = alias_find(reg, alias_name);
    if (!entry) return nullptr;

    entry->resolve_count++;
    return entry->target;
}

bool alias_exists(const AliasRegistry *reg, const char *alias_name) {
    if (!reg || !alias_name) return false;
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].active &&
            strcmp(reg->entries[i].alias_name, alias_name) == 0) {
            return true;
        }
    }
    return false;
}

int alias_update_target(AliasRegistry *reg, const char *alias_name, VaultNode *new_target) {
    if (!reg || !alias_name || !new_target) return -1;

    AliasEntry *entry = alias_find(reg, alias_name);
    if (!entry) return -1;

    entry->target = new_target;
    return 0;
}

int alias_count(const AliasRegistry *reg) {
    if (!reg) return 0;
    int count = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].active) count++;
    }
    return count;
}

int alias_list(const AliasRegistry *reg, char (*names)[ALIAS_NAME_MAX], int max_names) {
    if (!reg || !names) return -1;
    int listed = 0;
    for (int i = 0; i < reg->count && listed < max_names; i++) {
        if (reg->entries[i].active) {
            strncpy(names[listed], reg->entries[i].alias_name, ALIAS_NAME_MAX);
            listed++;
        }
    }
    return listed;
}

int alias_remove_by_target(AliasRegistry *reg, const VaultNode *target) {
    if (!reg || !target) return -1;
    int removed = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].active && reg->entries[i].target == target) {
            reg->entries[i].active = false;
            removed++;
        }
    }
    return removed;
}

int alias_validate(const AliasRegistry *reg) {
    if (!reg) return -1;
    int invalid = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].active && reg->entries[i].target == nullptr) {
            invalid++;
        }
    }
    return invalid;
}

void alias_clear(AliasRegistry *reg) {
    if (!reg) return;
    for (int i = 0; i < reg->count; i++) {
        reg->entries[i].active = false;
        reg->entries[i].target = nullptr;
    }
    reg->count = 0;
}

int alias_rename(AliasRegistry *reg, const char *old_name, const char *new_name) {
    if (!reg || !old_name || !new_name) return -1;

    AliasEntry *entry = alias_find(reg, old_name);
    if (!entry) return -1;

    if (alias_find(reg, new_name)) return -1;

    strncpy(entry->alias_name, new_name, ALIAS_NAME_MAX - 1);
    return 0;
}

static int alias_resolve_chain(AliasRegistry *reg, const char *alias_name,
                               VaultNode **chain, int max_depth, int *depth) {
    if (!reg || !alias_name || !chain || !depth) return -1;
    *depth = 0;

    const char *current = alias_name;
    for (int i = 0; i < max_depth; i++) {
        AliasEntry *entry = alias_find(reg, current);
        if (!entry) return -1;

        chain[*depth] = entry->target;
        (*depth)++;

        if (entry->target && entry->target->type != NODE_TYPE_ALIAS)
            return 0;

        if (entry->target && entry->target->data.alias_ref) {
            current = ((VaultNode *)entry->target->data.alias_ref)->name;
        } else {
            break;
        }
    }
    return -1;
}

static int alias_compact(AliasRegistry *reg) {
    if (!reg) return -1;
    int write_idx = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].active) {
            if (write_idx != i) {
                memcpy(&reg->entries[write_idx], &reg->entries[i], sizeof(AliasEntry));
            }
            write_idx++;
        }
    }
    int removed = reg->count - write_idx;
    reg->count = write_idx;
    return removed;
}

static bool alias_has_cycle(AliasRegistry *reg, const char *alias_name) {
    if (!reg || !alias_name) return false;

    const char *slow = alias_name;
    const char *fast = alias_name;

    for (int i = 0; i < ALIAS_MAX_ENTRIES; i++) {
        AliasEntry *slow_e = alias_find(reg, slow);
        if (!slow_e || !slow_e->target) break;
        slow = slow_e->target->name;

        AliasEntry *fast_e = alias_find(reg, fast);
        if (!fast_e || !fast_e->target) break;
        fast = fast_e->target->name;

        fast_e = alias_find(reg, fast);
        if (!fast_e || !fast_e->target) break;
        fast = fast_e->target->name;

        if (strcmp(slow, fast) == 0) return true;
    }
    return false;
}
