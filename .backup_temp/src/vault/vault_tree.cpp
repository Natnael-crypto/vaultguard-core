#include "vault/vault_tree.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

void vault_tree_init(VaultTree *tree, SecureArena *arena) {
    memset(tree, 0, sizeof(VaultTree));
    tree->arena = arena;
    tree->max_nodes = TREE_MAX_NODES;
    tree->next_node_id = 1;
    tree->tree_version = 1;

    tree->root = vault_node_create("root", NODE_TYPE_SUBVAULT);
    if (tree->root) {
        vault_node_init_subvault(tree->root, "root", 16);
        tree->node_count = 1;
    }
}

void vault_tree_destroy(VaultTree *tree) {
    if (!tree) return;
    if (tree->root) {
        vault_node_destroy(tree->root);
        free(tree->root);
        tree->root = nullptr;
    }
    tree->node_count = 0;
}

static VaultNode *tree_find_child(VaultNode *parent, const char *name) {
    if (!parent || parent->type != NODE_TYPE_SUBVAULT || !parent->data.sub_vault)
        return nullptr;

    for (int i = 0; i < parent->data.sub_vault->count; i++) {
        VaultNode *child = parent->data.sub_vault->entries[i];
        if (child && strcmp(child->name, name) == 0) return child;
    }
    return nullptr;
}

static VaultNode *tree_navigate(VaultTree *tree, const char *path, bool create_parents) {
    if (!tree || !tree->root || !path) return nullptr;

    char buf[NODE_NAME_MAX * 4];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    VaultNode *cur = tree->root;
    char *saveptr = nullptr;
    char *tok = strtok_r(buf, "/", &saveptr);

    while (tok) {
        char *next_tok = strtok_r(nullptr, "/", &saveptr);

        VaultNode *child = tree_find_child(cur, tok);
        if (!child) {
            if (create_parents && next_tok) {
                child = vault_node_create(tok, NODE_TYPE_SUBVAULT);
                if (!child) return nullptr;
                vault_node_init_subvault(child, tok, 8);
                vault_node_subvault_add(cur, child);
                tree->node_count++;
            } else if (!next_tok) {
                return cur;
            } else {
                return nullptr;
            }
        }

        cur = child;
        tok = next_tok;
    }

    return cur;
}

int vault_tree_insert(VaultTree *tree, const char *path, VaultNode *node) {
    if (!tree || !path || !node) return -1;
    if (tree->node_count >= tree->max_nodes) return -1;

    VaultNode *parent = tree_navigate(tree, path, true);
    if (!parent) return -1;

    if (parent->type != NODE_TYPE_SUBVAULT) return -1;

    vault_node_subvault_add(parent, node);
    tree->node_count++;
    tree->tree_version++;
    return 0;
}

VaultNode *vault_tree_lookup(VaultTree *tree, const char *path) {
    if (!tree || !path) return nullptr;

    char buf[NODE_NAME_MAX * 4];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    VaultNode *cur = tree->root;
    char *saveptr = nullptr;
    char *tok = strtok_r(buf, "/", &saveptr);

    while (tok) {
        VaultNode *child = tree_find_child(cur, tok);
        if (!child) return nullptr;
        cur = child;
        tok = strtok_r(nullptr, "/", &saveptr);
    }

    return cur;
}

int vault_tree_delete(VaultTree *tree, const char *path) {
    if (!tree || !path) return -1;

    char buf[NODE_NAME_MAX * 4];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *last_sep = strrchr(buf, '/');
    if (!last_sep) {
        VaultNode *child = tree_find_child(tree->root, buf);
        if (!child) return -1;

        for (int i = 0; i < tree->root->data.sub_vault->count; i++) {
            if (tree->root->data.sub_vault->entries[i] == child) {
                vault_node_destroy(child);
                free(child);
                vault_node_subvault_remove(tree->root, i);
                tree->node_count--;
                tree->tree_version++;
                return 0;
            }
        }
        return -1;
    }

    *last_sep = '\0';
    const char *child_name = last_sep + 1;

    VaultNode *parent = vault_tree_lookup(tree, buf);
    if (!parent || parent->type != NODE_TYPE_SUBVAULT) return -1;

    VaultNode *child = tree_find_child(parent, child_name);
    if (!child) return -1;

    for (int i = 0; i < parent->data.sub_vault->count; i++) {
        if (parent->data.sub_vault->entries[i] == child) {
            vault_node_destroy(child);
            free(child);
            vault_node_subvault_remove(parent, i);
            tree->node_count--;
            tree->tree_version++;
            return 0;
        }
    }
    return -1;
}

int vault_tree_delete_force(VaultTree *tree, const char *path) {
    return vault_tree_delete(tree, path);
}

int vault_tree_move(VaultTree *tree, const char *src, const char *dst) {
    if (!tree || !src || !dst) return -1;

    VaultNode *node = vault_tree_lookup(tree, src);
    if (!node) return -1;

    VaultNode *src_parent = node->parent;
    if (!src_parent || src_parent->type != NODE_TYPE_SUBVAULT) return -1;

    for (int i = 0; i < src_parent->data.sub_vault->count; i++) {
        if (src_parent->data.sub_vault->entries[i] == node) {
            vault_node_subvault_remove(src_parent, i);
            break;
        }
    }

    VaultNode *dst_parent = tree_navigate(tree, dst, true);
    if (!dst_parent || dst_parent->type != NODE_TYPE_SUBVAULT) return -1;

    vault_node_subvault_add(dst_parent, node);
    tree->tree_version++;
    return 0;
}

int vault_tree_merge_nodes(VaultTree *tree, VaultNode *dst, VaultNode *src) {
    if (!tree || !dst || !src) return -1;

    if (dst->type != NODE_TYPE_SUBVAULT || src->type != NODE_TYPE_SUBVAULT)
        return -1;

    if (!dst->data.sub_vault || !src->data.sub_vault) return -1;

    int original_count = dst->data.sub_vault->count;
    uint8_t *original_key_ptr = nullptr;
    uint32_t original_type = dst->type;

    for (int i = 0; i < src->data.sub_vault->count; i++) {
        VaultNode *src_child = src->data.sub_vault->entries[i];
        if (!src_child) continue;

        VaultNode *existing = tree_find_child(dst, src_child->name);

        if (existing && existing->type == NODE_TYPE_SUBVAULT &&
            src_child->type == NODE_TYPE_SUBVAULT) {
            vault_tree_merge_nodes(tree, existing, src_child);
        } else {
            VaultNode *copy = nullptr;
            int rc = vault_node_deep_copy(&copy, src_child);
            if (rc != 0) {
                for (int j = dst->data.sub_vault->count - 1;
                     j >= original_count; j--) {
                    VaultNode *added = dst->data.sub_vault->entries[j];
                    if (added) {
                        dst->data.sub_vault->entries[j] = nullptr;
                        dst->data.sub_vault->count--;
                    }
                }

                dst->data.sym_key = original_key_ptr;

                return -1;
            }
            vault_node_subvault_add(dst, copy);
        }
    }

    tree->tree_version++;
    return 0;
}

int vault_tree_split(VaultTree *tree, VaultNode *node, const char *new_path) {
    if (!tree || !node || !new_path) return -1;
    if (node->type != NODE_TYPE_SUBVAULT || !node->data.sub_vault) return -1;
    if (node->data.sub_vault->count < 2) return -1;

    VaultNode *new_vault = vault_node_create("split", NODE_TYPE_SUBVAULT);
    if (!new_vault) return -1;

    vault_node_init_subvault(new_vault, "split", 8);

    int half = node->data.sub_vault->count / 2;
    for (int i = node->data.sub_vault->count - 1; i >= half; i--) {
        VaultNode *child = node->data.sub_vault->entries[i];
        vault_node_subvault_add(new_vault, child);
        node->data.sub_vault->entries[i] = nullptr;
        node->data.sub_vault->count--;
    }

    vault_tree_insert(tree, new_path, new_vault);
    return 0;
}

int vault_tree_count(const VaultTree *tree) {
    if (!tree) return 0;
    return tree->node_count;
}

int vault_tree_depth(const VaultTree *tree, const char *path) {
    if (!tree || !path) return 0;
    int depth = 0;
    for (const char *p = path; *p; p++) {
        if (*p == TREE_PATH_SEPARATOR) depth++;
    }
    return depth;
}

VaultNode *vault_tree_find_parent(VaultTree *tree, const char *path) {
    if (!tree || !path) return nullptr;

    char buf[NODE_NAME_MAX * 4];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *last = strrchr(buf, '/');
    if (!last) return tree->root;
    *last = '\0';
    return vault_tree_lookup(tree, buf);
}

int vault_tree_path_resolve(const VaultTree *tree, const char *path,
                            VaultNode **segments, int max_segments) {
    if (!tree || !path || !segments) return -1;

    char buf[NODE_NAME_MAX * 4];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int count = 0;
    VaultNode *cur = tree->root;
    segments[count++] = cur;

    char *saveptr = nullptr;
    char *tok = strtok_r(buf, "/", &saveptr);
    while (tok && count < max_segments) {
        VaultNode *child = tree_find_child(cur, tok);
        if (!child) break;
        segments[count++] = child;
        cur = child;
        tok = strtok_r(nullptr, "/", &saveptr);
    }

    return count;
}

void vault_iterator_init(VaultIterator *it, VaultNode *root) {
    memset(it, 0, sizeof(VaultIterator));
    it->current_node = root;
    it->stack[0] = root;
    it->stack_indices[0] = 0;
    it->stack_depth = 1;
    it->finished = false;
}

VaultNode *vault_iterator_next(VaultIterator *it) {
    if (!it || it->finished) return nullptr;

    while (it->stack_depth > 0) {
        int top = it->stack_depth - 1;
        VaultNode *node = it->stack[top];
        int idx = it->stack_indices[top];

        if (!node) {
            it->stack_depth--;
            continue;
        }

        if (node->type == NODE_TYPE_SUBVAULT && node->data.sub_vault &&
            idx < node->data.sub_vault->count) {
            it->stack_indices[top] = idx + 1;

            VaultNode *child = node->data.sub_vault->entries[idx];
            if (child && it->stack_depth < TREE_MAX_DEPTH) {
                it->stack[it->stack_depth] = child;
                it->stack_indices[it->stack_depth] = 0;
                it->stack_depth++;
                it->current_node = child;
                return child;
            }
        } else {
            it->stack_depth--;
            if (it->stack_depth > 0) {
                it->current_node = it->stack[it->stack_depth - 1];
            }
        }
    }

    it->finished = true;
    it->current_node = nullptr;
    return nullptr;
}

bool vault_iterator_has_next(const VaultIterator *it) {
    if (!it) return false;
    return !it->finished;
}

void vault_iterator_reset(VaultIterator *it, VaultNode *root) {
    vault_iterator_init(it, root);
}

void vault_iterator_destroy(VaultIterator *it, SecureArena *arena) {
    if (!it) return;

    if (it->current_node && arena) {
        arena_free(arena, it->current_node, sizeof(VaultNode));
    }

    memset(it, 0, sizeof(VaultIterator));
    it->finished = true;
}

int vault_tree_walk(VaultTree *tree,
                    int (*visitor)(VaultNode *node, void *ctx), void *ctx) {
    if (!tree || !visitor || !tree->root) return -1;

    VaultIterator it;
    vault_iterator_init(&it, tree->root);

    VaultNode *node;
    int visited = 0;
    while ((node = vault_iterator_next(&it)) != nullptr) {
        if (visitor(node, ctx) != 0) break;
        visited++;
    }

    return visited;
}

int vault_tree_serialize(const VaultTree *tree, uint8_t *out, size_t max_len,
                         size_t *written) {
    if (!tree || !out || !written) return -1;

    size_t pos = 0;

    auto write_u32 = [&](uint32_t val) -> int {
        if (pos + 4 > max_len) return -1;
        memcpy(out + pos, &val, 4);
        pos += 4;
        return 0;
    };

    auto write_str = [&](const char *s) -> int {
        size_t len = strlen(s);
        if (pos + 4 + len > max_len) return -1;
        uint32_t l = (uint32_t)len;
        memcpy(out + pos, &l, 4);
        pos += 4;
        memcpy(out + pos, s, len);
        pos += len;
        return 0;
    };

    write_u32(0x56475244);
    write_u32((uint32_t)tree->node_count);
    write_u32((uint32_t)tree->tree_version);

    VaultIterator it;
    vault_iterator_init(&it, tree->root);
    VaultNode *node;
    while ((node = vault_iterator_next(&it)) != nullptr) {
        write_u32(node->type);
        write_str(node->name);
        write_u32((uint32_t)node->data_size);
        write_u32(node->flags);
    }

    *written = pos;
    return 0;
}

int vault_tree_deserialize(VaultTree *tree, const uint8_t *data, size_t len) {
    if (!tree || !data || len < 12) return -1;

    uint32_t magic;
    memcpy(&magic, data, 4);
    if (magic != 0x56475244) return -1;

    return 0;
}

void vault_tree_lock(VaultTree *tree) {
    if (tree) tree->locked = true;
}

void vault_tree_unlock(VaultTree *tree) {
    if (tree) tree->locked = false;
}

int vault_tree_verify_integrity(const VaultTree *tree) {
    if (!tree || !tree->root) return -1;

    VaultIterator it;
    vault_iterator_init(&it, tree->root);
    VaultNode *node;
    int errors = 0;

    while ((node = vault_iterator_next(&it)) != nullptr) {
        if (node->type < NODE_TYPE_KEY || node->type > NODE_TYPE_ALIAS) {
            errors++;
        }
        if (node->ref_count < 0) errors++;
    }

    return errors;
}

uint64_t vault_tree_version(const VaultTree *tree) {
    if (!tree) return 0;
    return tree->tree_version;
}

static int tree_count_nodes_recursive(const VaultNode *node) {
    if (!node) return 0;
    int count = 1;
    if (node->type == NODE_TYPE_SUBVAULT && node->data.sub_vault) {
        for (int i = 0; i < node->data.sub_vault->count; i++) {
            count += tree_count_nodes_recursive(node->data.sub_vault->entries[i]);
        }
    }
    return count;
}

static int tree_max_depth(const VaultNode *node, int current) {
    if (!node) return current;
    int max_d = current;
    if (node->type == NODE_TYPE_SUBVAULT && node->data.sub_vault) {
        for (int i = 0; i < node->data.sub_vault->count; i++) {
            int d = tree_max_depth(node->data.sub_vault->entries[i], current + 1);
            if (d > max_d) max_d = d;
        }
    }
    return max_d;
}

static VaultNode *tree_find_by_id(VaultNode *node, uint32_t node_id) {
    if (!node) return nullptr;
    if (node->node_id == node_id) return node;
    if (node->type == NODE_TYPE_SUBVAULT && node->data.sub_vault) {
        for (int i = 0; i < node->data.sub_vault->count; i++) {
            VaultNode *found = tree_find_by_id(node->data.sub_vault->entries[i], node_id);
            if (found) return found;
        }
    }
    return nullptr;
}

static void tree_collect_keys(const VaultNode *node, VaultNode **keys, int *count, int max_keys) {
    if (!node || *count >= max_keys) return;
    if (node->type == NODE_TYPE_KEY) {
        keys[*count] = (VaultNode *)node;
        (*count)++;
    }
    if (node->type == NODE_TYPE_SUBVAULT && node->data.sub_vault) {
        for (int i = 0; i < node->data.sub_vault->count; i++) {
            tree_collect_keys(node->data.sub_vault->entries[i], keys, count, max_keys);
        }
    }
}

static size_t tree_total_data_size(const VaultNode *node) {
    if (!node) return 0;
    size_t total = node->data_size;
    if (node->type == NODE_TYPE_SUBVAULT && node->data.sub_vault) {
        for (int i = 0; i < node->data.sub_vault->count; i++) {
            total += tree_total_data_size(node->data.sub_vault->entries[i]);
        }
    }
    return total;
}
