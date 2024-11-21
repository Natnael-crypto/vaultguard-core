#ifndef VAULTGUARD_VAULT_ALIAS_HPP
#define VAULTGUARD_VAULT_ALIAS_HPP

#include <cstddef>
#include <cstdint>
#include "vault/vault_node.hpp"

#define ALIAS_MAX_ENTRIES     256
#define ALIAS_NAME_MAX        128

struct AliasEntry {
    char        alias_name[ALIAS_NAME_MAX];
    char        target_path[ALIAS_NAME_MAX * 4];
    VaultNode  *target;
    uint64_t    created_at;
    uint32_t    alias_id;
    bool        active;
    int         resolve_count;
};

struct AliasRegistry {
    AliasEntry  entries[ALIAS_MAX_ENTRIES];
    int         count;
    uint32_t    next_id;
    bool        initialized;
};

void        alias_registry_init(AliasRegistry *reg);
void        alias_registry_destroy(AliasRegistry *reg);
int         alias_create(AliasRegistry *reg, const char *alias_name,
                         const char *target_path, VaultNode *target);
int         alias_remove(AliasRegistry *reg, const char *alias_name);
VaultNode  *alias_resolve(AliasRegistry *reg, const char *alias_name);
bool        alias_exists(const AliasRegistry *reg, const char *alias_name);
int         alias_update_target(AliasRegistry *reg, const char *alias_name, VaultNode *new_target);
int         alias_count(const AliasRegistry *reg);
int         alias_list(const AliasRegistry *reg, char (*names)[ALIAS_NAME_MAX], int max_names);
AliasEntry *alias_find(AliasRegistry *reg, const char *alias_name);
int         alias_remove_by_target(AliasRegistry *reg, const VaultNode *target);
int         alias_validate(const AliasRegistry *reg);
void        alias_clear(AliasRegistry *reg);
int         alias_rename(AliasRegistry *reg, const char *old_name, const char *new_name);

#endif
