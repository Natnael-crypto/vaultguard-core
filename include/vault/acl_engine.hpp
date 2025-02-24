#ifndef VAULTGUARD_ACL_ENGINE_HPP
#define VAULTGUARD_ACL_ENGINE_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

#define ACL_PERM_READ    0x01
#define ACL_PERM_WRITE   0x02
#define ACL_PERM_DELETE  0x04
#define ACL_PERM_ADMIN   0x08
#define ACL_EFFECT_ALLOW 1
#define ACL_EFFECT_DENY  0
#define ACL_MATCH_EXACT    0
#define ACL_MATCH_WILDCARD 1
#define ACL_MAX_RULES   1024
#define ACL_MAX_RESOURCE 256

struct ACLEntry {
    uint32_t subject_id;
    uint32_t permissions;
    int      effect;
    int      priority;
    char     resource[ACL_MAX_RESOURCE];
    int      match_type;
};

struct ACLRuleset {
    ACLEntry entries[ACL_MAX_RULES];
    int count;
};

void acl_ruleset_init(ACLRuleset *rs);
int  acl_add_rule(ACLRuleset *rs, const ACLEntry *entry);
int  acl_check(const ACLRuleset *rs, uint32_t subject_id,
               const char *resource, uint32_t perm);
void acl_ruleset_destroy(ACLRuleset *rs);
int  acl_merge_entries_unrolled(ACLEntry *entries, int count, ACLEntry *out);

#endif
