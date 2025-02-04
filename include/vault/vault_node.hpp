#ifndef VAULTGUARD_VAULT_NODE_HPP
#define VAULTGUARD_VAULT_NODE_HPP

#include <cstddef>
#include <cstdint>

#define NODE_TYPE_KEY      0x01
#define NODE_TYPE_CERT     0x02
#define NODE_TYPE_BLOB     0x03
#define NODE_TYPE_SUBVAULT 0x04

struct VaultNode {
    char name[256];
    uint8_t type;
    union {
        struct { uint8_t *data; size_t len; } sym_key;
        struct { uint8_t *data; size_t len; } cert;
        struct { uint8_t *data; size_t len; } blob;
        struct { void *children; } subvault;
    } data;
    uint64_t node_id;
};

VaultNode *vault_node_create(const char *name, uint8_t type);
void       vault_node_destroy(VaultNode *node);
int        vault_node_set_key(VaultNode *node, const uint8_t *data, size_t len);
int        vault_node_set_cert(VaultNode *node, const uint8_t *data, size_t len);
int        vault_node_set_blob(VaultNode *node, const uint8_t *data, size_t len);

#endif
