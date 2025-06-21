#include "vault/vault_node.hpp"
#include "allocator/secure_arena.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

static uint32_t next_global_node_id = 1;

VaultNode *vault_node_create(const char *name, uint32_t type) {
    VaultNode *node = (VaultNode *)malloc(sizeof(VaultNode));
    if (!node) return nullptr;

    memset(node, 0, sizeof(VaultNode));
    if (name) strncpy(node->name, name, NODE_NAME_MAX - 1);
    node->type = type;
    node->node_id = next_global_node_id++;
    node->ref_count = 1;
    node->version = 1;
    return node;
}

void vault_node_destroy(VaultNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_TYPE_KEY:
            if (node->data.sym_key) {
                secure_memzero(node->data.sym_key, node->data_size);
                free(node->data.sym_key);
                node->data.sym_key = nullptr;
            }
            break;

        case NODE_TYPE_CERT:
            if (node->data.cert_der) {
                free(node->data.cert_der);
                node->data.cert_der = nullptr;
            }
            break;

        case NODE_TYPE_SUBVAULT:
            if (node->data.sub_vault) {
                for (int i = 0; i < node->data.sub_vault->count; i++) {
                    if (node->data.sub_vault->entries[i]) {
                        vault_node_destroy(node->data.sub_vault->entries[i]);
                        free(node->data.sub_vault->entries[i]);
                    }
                }
                free(node->data.sub_vault->entries);
                free(node->data.sub_vault);
                node->data.sub_vault = nullptr;
            }
            break;

        case NODE_TYPE_BLOB:
            if (node->data.blob_data) {
                free(node->data.blob_data);
                node->data.blob_data = nullptr;
            }
            break;

        case NODE_TYPE_ALIAS:
            node->data.alias_ref = nullptr;
            break;
    }

    node->data_size = 0;
}

int vault_node_set_key(VaultNode *node, const uint8_t *key_data, size_t len) {
    if (!node || !key_data || len == 0) return -1;
    if (node->type != NODE_TYPE_KEY) return -1;

    if (node->data.sym_key) {
        secure_memzero(node->data.sym_key, node->data_size);
        free(node->data.sym_key);
    }

    node->data.sym_key = (uint8_t *)malloc(len);
    if (!node->data.sym_key) return -1;

    memcpy(node->data.sym_key, key_data, len);
    node->data_size = len;
    node->version++;
    return 0;
}

int vault_node_set_cert(VaultNode *node, const uint8_t *cert_data, size_t len) {
    if (!node || !cert_data || len == 0) return -1;
    if (node->type != NODE_TYPE_CERT) return -1;

    if (node->data.cert_der) free(node->data.cert_der);

    node->data.cert_der = (uint8_t *)malloc(len);
    if (!node->data.cert_der) return -1;

    memcpy(node->data.cert_der, cert_data, len);
    node->data_size = len;
    node->version++;
    return 0;
}

int vault_node_set_blob(VaultNode *node, const uint8_t *blob, size_t len) {
    if (!node || !blob || len == 0) return -1;
    if (node->type != NODE_TYPE_BLOB) return -1;

    if (node->data.blob_data) free(node->data.blob_data);

    node->data.blob_data = (uint8_t *)malloc(len);
    if (!node->data.blob_data) return -1;

    memcpy(node->data.blob_data, blob, len);
    node->data_size = len;
    node->version++;
    return 0;
}

int vault_node_init_subvault(VaultNode *node, const char *label, int initial_cap) {
    if (!node) return -1;

    node->type = NODE_TYPE_SUBVAULT;
    node->data.sub_vault = (SubVault *)malloc(sizeof(SubVault));
    if (!node->data.sub_vault) return -1;

    memset(node->data.sub_vault, 0, sizeof(SubVault));
    if (label) strncpy(node->data.sub_vault->label, label, NODE_NAME_MAX - 1);

    if (initial_cap < 4) initial_cap = 4;
    node->data.sub_vault->entries = (VaultNode **)calloc((size_t)initial_cap, sizeof(VaultNode *));
    if (!node->data.sub_vault->entries) {
        free(node->data.sub_vault);
        node->data.sub_vault = nullptr;
        return -1;
    }

    node->data.sub_vault->count = 0;
    node->data.sub_vault->capacity = initial_cap;
    return 0;
}

int vault_node_subvault_add(VaultNode *node, VaultNode *child) {
    if (!node || !child) return -1;
    if (node->type != NODE_TYPE_SUBVAULT || !node->data.sub_vault) return -1;

    SubVault *sv = node->data.sub_vault;
    if (sv->count >= sv->capacity) {
        int new_cap = sv->capacity * 2;
        VaultNode **new_entries = (VaultNode **)realloc(sv->entries,
                                                        (size_t)new_cap * sizeof(VaultNode *));
        if (!new_entries) return -1;
        sv->entries = new_entries;
        sv->capacity = new_cap;
    }

    child->parent = node;
    sv->entries[sv->count++] = child;
    return 0;
}

int vault_node_subvault_remove(VaultNode *node, int index) {
    if (!node || node->type != NODE_TYPE_SUBVAULT || !node->data.sub_vault) return -1;

    SubVault *sv = node->data.sub_vault;
    if (index < 0 || index >= sv->count) return -1;

    for (int i = index; i < sv->count - 1; i++) {
        sv->entries[i] = sv->entries[i + 1];
    }
    sv->entries[sv->count - 1] = nullptr;
    sv->count--;
    return 0;
}

VaultNode *vault_node_subvault_get(const VaultNode *node, int index) {
    if (!node || node->type != NODE_TYPE_SUBVAULT || !node->data.sub_vault) return nullptr;
    if (index < 0 || index >= node->data.sub_vault->count) return nullptr;
    return node->data.sub_vault->entries[index];
}

int vault_node_subvault_count(const VaultNode *node) {
    if (!node || node->type != NODE_TYPE_SUBVAULT || !node->data.sub_vault) return 0;
    return node->data.sub_vault->count;
}

int vault_node_set_metadata(VaultNode *node, const char *key, const char *value) {
    if (!node || !key || !value) return -1;

    for (int i = 0; i < node->metadata_count; i++) {
        if (strcmp(node->metadata[i].key, key) == 0) {
            strncpy(node->metadata[i].value, value, sizeof(node->metadata[i].value) - 1);
            return 0;
        }
    }

    if (node->metadata_count >= NODE_MAX_METADATA) return -1;

    NodeMetadata *md = &node->metadata[node->metadata_count];
    strncpy(md->key, key, sizeof(md->key) - 1);
    strncpy(md->value, value, sizeof(md->value) - 1);
    node->metadata_count++;
    return 0;
}

const char *vault_node_get_metadata(const VaultNode *node, const char *key) {
    if (!node || !key) return nullptr;
    for (int i = 0; i < node->metadata_count; i++) {
        if (strcmp(node->metadata[i].key, key) == 0)
            return node->metadata[i].value;
    }
    return nullptr;
}

int vault_node_copy(VaultNode *dst, const VaultNode *src) {
    if (!dst || !src) return -1;
    memcpy(dst, src, sizeof(VaultNode));
    dst->ref_count = 1;
    return 0;
}

int vault_node_deep_copy(VaultNode **dst, const VaultNode *src) {
    if (!dst || !src) return -1;

    *dst = vault_node_create(src->name, src->type);
    if (!*dst) return -1;

    VaultNode *d = *dst;
    d->flags = src->flags;
    d->version = src->version;
    d->metadata_count = src->metadata_count;
    memcpy(d->metadata, src->metadata, sizeof(src->metadata));

    switch (src->type) {
        case NODE_TYPE_KEY:
            if (src->data.sym_key && src->data_size > 0) {
                vault_node_set_key(d, src->data.sym_key, src->data_size);
            }
            break;
        case NODE_TYPE_CERT:
            if (src->data.cert_der && src->data_size > 0) {
                vault_node_set_cert(d, src->data.cert_der, src->data_size);
            }
            break;
        case NODE_TYPE_BLOB:
            if (src->data.blob_data && src->data_size > 0) {
                vault_node_set_blob(d, src->data.blob_data, src->data_size);
            }
            break;
        case NODE_TYPE_SUBVAULT:
            if (src->data.sub_vault) {
                vault_node_init_subvault(d, src->data.sub_vault->label,
                                         src->data.sub_vault->capacity);
                for (int i = 0; i < src->data.sub_vault->count; i++) {
                    VaultNode *child_copy = nullptr;
                    if (vault_node_deep_copy(&child_copy, src->data.sub_vault->entries[i]) == 0) {
                        vault_node_subvault_add(d, child_copy);
                    }
                }
            }
            break;
    }

    return 0;
}

void vault_node_add_ref(VaultNode *node) {
    if (!node) return;
    node->ref_count++;
}

void vault_node_release(VaultNode *node) {
    if (!node) return;
    node->ref_count--;
    if (node->ref_count <= 0) {
        vault_node_destroy(node);
        free(node);
    }
}

bool vault_node_is_container(const VaultNode *node) {
    if (!node) return false;
    return node->type == NODE_TYPE_SUBVAULT;
}

size_t vault_node_total_size(const VaultNode *node) {
    if (!node) return 0;
    size_t total = sizeof(VaultNode) + node->data_size;
    if (node->type == NODE_TYPE_SUBVAULT && node->data.sub_vault) {
        for (int i = 0; i < node->data.sub_vault->count; i++) {
            total += vault_node_total_size(node->data.sub_vault->entries[i]);
        }
    }
    return total;
}

uint32_t vault_node_compute_hash(const VaultNode *node) {
    if (!node) return 0;
    uint32_t hash = 0x811C9DC5;
    const uint8_t *name_bytes = (const uint8_t *)node->name;
    for (int i = 0; name_bytes[i]; i++) {
        hash ^= name_bytes[i];
        hash *= 0x01000193;
    }
    hash ^= node->type;
    hash *= 0x01000193;
    hash ^= node->node_id;
    return hash;
}

static int vault_node_validate(const VaultNode *node) {
    if (!node) return -1;
    if (node->type < NODE_TYPE_KEY || node->type > NODE_TYPE_ALIAS) return -1;
    if (node->ref_count < 0) return -1;
    return 0;
}

static int vault_node_count_descendants(const VaultNode *node) {
    if (!node) return 0;
    int count = 1;
    if (node->type == NODE_TYPE_SUBVAULT && node->data.sub_vault) {
        for (int i = 0; i < node->data.sub_vault->count; i++) {
            count += vault_node_count_descendants(node->data.sub_vault->entries[i]);
        }
    }
    return count;
}
