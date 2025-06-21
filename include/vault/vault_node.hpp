#ifndef VAULTGUARD_VAULT_NODE_HPP
#define VAULTGUARD_VAULT_NODE_HPP

#include <cstddef>
#include <cstdint>

#define NODE_TYPE_KEY        0x01
#define NODE_TYPE_CERT       0x02
#define NODE_TYPE_SUBVAULT   0x03
#define NODE_TYPE_BLOB       0x04
#define NODE_TYPE_ALIAS      0x05
#define NODE_NAME_MAX        128
#define NODE_MAX_METADATA    16
#define NODE_FLAG_READONLY   0x01
#define NODE_FLAG_ENCRYPTED  0x02
#define NODE_FLAG_SYSTEM     0x04
#define NODE_FLAG_TEMPORARY  0x08

struct VaultNode;

struct SubVault {
    VaultNode **entries;
    int         count;
    int         capacity;
    char        label[NODE_NAME_MAX];
    uint32_t    sub_flags;
};

struct NodeMetadata {
    char     key[64];
    char     value[256];
};

union NodeData {
    uint8_t    *sym_key;
    uint8_t    *cert_der;
    SubVault   *sub_vault;
    uint8_t    *blob_data;
    void       *alias_ref;
};

struct VaultNode {
    char          name[NODE_NAME_MAX];
    uint32_t      type;
    NodeData      data;
    size_t        data_size;
    uint32_t      flags;
    uint32_t      node_id;
    uint64_t      created_at;
    uint64_t      modified_at;
    uint32_t      access_count;
    uint32_t      version;
    NodeMetadata  metadata[NODE_MAX_METADATA];
    int           metadata_count;
    VaultNode    *parent;
    int           ref_count;
};

VaultNode *vault_node_create(const char *name, uint32_t type);
void       vault_node_destroy(VaultNode *node);
int        vault_node_set_key(VaultNode *node, const uint8_t *key_data, size_t len);
int        vault_node_set_cert(VaultNode *node, const uint8_t *cert_data, size_t len);
int        vault_node_set_blob(VaultNode *node, const uint8_t *blob, size_t len);
int        vault_node_init_subvault(VaultNode *node, const char *label, int initial_cap);
int        vault_node_subvault_add(VaultNode *node, VaultNode *child);
int        vault_node_subvault_remove(VaultNode *node, int index);
VaultNode *vault_node_subvault_get(const VaultNode *node, int index);
int        vault_node_subvault_count(const VaultNode *node);
int        vault_node_set_metadata(VaultNode *node, const char *key, const char *value);
const char*vault_node_get_metadata(const VaultNode *node, const char *key);
int        vault_node_copy(VaultNode *dst, const VaultNode *src);
int        vault_node_deep_copy(VaultNode **dst, const VaultNode *src);
void       vault_node_add_ref(VaultNode *node);
void       vault_node_release(VaultNode *node);
bool       vault_node_is_container(const VaultNode *node);
size_t     vault_node_total_size(const VaultNode *node);
uint32_t   vault_node_compute_hash(const VaultNode *node);

#endif
