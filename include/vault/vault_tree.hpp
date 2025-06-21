#ifndef VAULTGUARD_VAULT_TREE_HPP
#define VAULTGUARD_VAULT_TREE_HPP

#include <cstddef>
#include <cstdint>
#include "vault/vault_node.hpp"
#include "allocator/secure_arena.hpp"

#define TREE_MAX_DEPTH         32
#define TREE_MAX_NODES         4096
#define TREE_PATH_SEPARATOR    '/'

struct VaultIterator {
    VaultNode  *current_node;
    VaultNode  *stack[TREE_MAX_DEPTH];
    int         stack_indices[TREE_MAX_DEPTH];
    int         stack_depth;
    bool        finished;
    uint64_t    iterator_id;
};

struct VaultTree {
    VaultNode    *root;
    SecureArena  *arena;
    int           node_count;
    int           max_nodes;
    uint64_t      next_node_id;
    uint64_t      tree_version;
    bool          locked;
};

void          vault_tree_init(VaultTree *tree, SecureArena *arena);
void          vault_tree_destroy(VaultTree *tree);
int           vault_tree_insert(VaultTree *tree, const char *path, VaultNode *node);
VaultNode    *vault_tree_lookup(VaultTree *tree, const char *path);
int           vault_tree_delete(VaultTree *tree, const char *path);
int           vault_tree_delete_force(VaultTree *tree, const char *path);
int           vault_tree_move(VaultTree *tree, const char *src, const char *dst);
int           vault_tree_merge_nodes(VaultTree *tree, VaultNode *dst, VaultNode *src);
int           vault_tree_split(VaultTree *tree, VaultNode *node, const char *new_path);
int           vault_tree_count(const VaultTree *tree);
int           vault_tree_depth(const VaultTree *tree, const char *path);
VaultNode    *vault_tree_find_parent(VaultTree *tree, const char *path);
int           vault_tree_path_resolve(const VaultTree *tree, const char *path,
                                      VaultNode **segments, int max_segments);
void          vault_iterator_init(VaultIterator *it, VaultNode *root);
void          vault_iterator_destroy(VaultIterator *it, SecureArena *arena);
VaultNode    *vault_iterator_next(VaultIterator *it);
bool          vault_iterator_has_next(const VaultIterator *it);
void          vault_iterator_reset(VaultIterator *it, VaultNode *root);
int           vault_tree_walk(VaultTree *tree,
                              int (*visitor)(VaultNode *node, void *ctx), void *ctx);
int           vault_tree_serialize(const VaultTree *tree, uint8_t *out, size_t max_len,
                                   size_t *written);
int           vault_tree_deserialize(VaultTree *tree, const uint8_t *data, size_t len);
void          vault_tree_lock(VaultTree *tree);
void          vault_tree_unlock(VaultTree *tree);
int           vault_tree_verify_integrity(const VaultTree *tree);
uint64_t      vault_tree_version(const VaultTree *tree);

#endif
