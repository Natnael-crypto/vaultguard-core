#include <cassert>
#include <cstdio>
#include <cstring>
#include "vault/vault_tree.hpp"
#include "vault/vault_node.hpp"
#include "vault/vault_alias.hpp"
#include "vault/acl_engine.hpp"

void test_vault_tree() {
    SecureArena arena;
    arena_init(&arena);

    VaultTree tree;
    vault_tree_init(&tree, &arena);

    VaultNode *key1 = vault_node_create("key1", NODE_TYPE_KEY);
    assert(key1 != nullptr);
    uint8_t key_data[] = {1, 2, 3, 4};
    vault_node_set_key(key1, key_data, 4);

    assert(vault_tree_insert(&tree, "keys", key1) == 0);
    
    VaultNode *found = vault_tree_lookup(&tree, "keys/key1");
    assert(found == key1);

    assert(vault_tree_count(&tree) == 3); // root, keys, key1

    vault_tree_destroy(&tree);
    arena_destroy(&arena);
}

void test_acl() {
    ACLRuleset rs;
    acl_ruleset_init(&rs);

    ACLEntry e;
    memset(&e, 0, sizeof(e));
    e.subject_id = 1;
    e.permissions = 3;
    e.effect = ACL_EFFECT_ALLOW;
    e.active = true;
    strncpy(e.resource, "/keys/key1", sizeof(e.resource));
    e.match_type = ACL_MATCH_EXACT;

    acl_add_rule(&rs, &e);

    assert(acl_check(&rs, 1, "/keys/key1", 1) == 1);
    assert(acl_check(&rs, 1, "/keys/key1", 4) == 0);
    assert(acl_check(&rs, 2, "/keys/key1", 1) == 0);

    acl_ruleset_destroy(&rs);
}

void test_alias() {
    AliasRegistry reg;
    alias_registry_init(&reg);

    VaultNode dummy;
    memset(&dummy, 0, sizeof(dummy));
    
    assert(alias_create(&reg, "alias1", "/path", &dummy) == 0);
    assert(alias_resolve(&reg, "alias1") == &dummy);
    assert(alias_remove(&reg, "alias1") == 0);
    assert(alias_resolve(&reg, "alias1") == nullptr);

    alias_registry_destroy(&reg);
}

int main() {
    test_vault_tree();
    test_acl();
    test_alias();
    printf("test_vault passed\n");
    return 0;
}
