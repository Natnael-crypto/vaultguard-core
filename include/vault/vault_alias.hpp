#ifndef VAULTGUARD_VAULT_ALIAS_HPP
#define VAULTGUARD_VAULT_ALIAS_HPP

#include <cstddef>
#include <cstdint>

struct AliasEntry {
    char alias[256];
    char target_path[512];
    bool active;
};

struct AliasRegistry {
    AliasEntry entries[1024];
    int count;
};

void alias_registry_init(AliasRegistry *reg);
int  alias_register(AliasRegistry *reg, const char *alias, const char *target);
const char *alias_resolve(const AliasRegistry *reg, const char *alias);
int  alias_remove(AliasRegistry *reg, const char *alias);

#endif
