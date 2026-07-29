#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "allocator/secure_arena.hpp"
#include "tlv/tlv_decoder.hpp"
#include "tlv/tlv_encoder.hpp"
#include "vault/vault_node.hpp"
#include "vault/vault_tree.hpp"
#include "vault/vault_alias.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    uint8_t mode = data[0];
    uint8_t sub_mode = data[1];
    const uint8_t *payload = data + 2;
    size_t payload_len = size - 2;

    SecureArena arena;
    arena_init(&arena);

    if ((mode % 5) == 0) {
        TLVDecoder dec;
        tlv_decoder_init(&dec, payload, payload_len);
        TLVNode *root = nullptr;
        tlv_decode(&dec, &root);
        tlv_decoder_destroy(&dec);
    }

    if ((mode % 5) == 1) {
        VaultTree tree;
        vault_tree_init(&tree, &arena);

        int num_nodes = (int)(payload[0] % 16) + 1;
        for (int i = 0; i < num_nodes && i * 4 + 4 <= (int)payload_len; i++) {
            uint32_t ntype = (payload[i * 4] % 4) + 1;
            char name[32];
            snprintf(name, sizeof(name), "node_%d", i);

            VaultNode *node = vault_node_create(name, ntype);
            if (!node) continue;

            if (ntype == NODE_TYPE_KEY && i * 4 + 8 <= (int)payload_len) {
                vault_node_set_key(node, payload + i * 4 + 1, 4);
            } else if (ntype == NODE_TYPE_BLOB && i * 4 + 8 <= (int)payload_len) {
                vault_node_set_blob(node, payload + i * 4 + 1, 4);
            }

            vault_tree_insert(&tree, "data", node);
        }

        VaultNode *key_node = vault_node_create("merge-key", NODE_TYPE_KEY);
        if (key_node) {
            uint8_t kd[] = {0xAA, 0xBB, 0xCC, 0xDD};
            vault_node_set_key(key_node, kd, sizeof(kd));
            vault_tree_insert(&tree, "data", key_node);
        }

        if (payload_len > 4 && (sub_mode % 4) == 0) {
            VaultNode *src = vault_node_create("merge-src", NODE_TYPE_SUBVAULT);
            if (src) {
                vault_node_init_subvault(src, "merge-src", 4);
                VaultNode *child = vault_node_create("child", NODE_TYPE_KEY);
                if (child) {
                    vault_node_set_key(child, payload, payload_len > 32 ? 32 : payload_len);
                    vault_node_subvault_add(src, child);
                }

                VaultNode *dst = vault_tree_lookup(&tree, "data");
                if (dst && dst->type == NODE_TYPE_SUBVAULT) {
                    vault_tree_merge_nodes(&tree, dst, src);
                }

                vault_node_destroy(src);
                free(src);
            }
        }

        vault_tree_destroy(&tree);
    }

    if ((mode % 5) == 2) {
        VaultTree tree;
        vault_tree_init(&tree, &arena);
        AliasRegistry aliases;
        alias_registry_init(&aliases);

        VaultNode *target = vault_node_create("target-vault", NODE_TYPE_SUBVAULT);
        if (target) {
            vault_node_init_subvault(target, "target", 4);
            vault_tree_insert(&tree, "vaults", target);

            alias_create(&aliases, "my-alias", "vaults/target-vault", target);

            if (payload_len > 2 && (sub_mode % 3) == 0) {
                vault_tree_delete_force(&tree, "vaults/target-vault");

                VaultNode *resolved = alias_resolve(&aliases, "my-alias");
                if (resolved) {
                    volatile uint32_t flags = resolved->flags;
                    (void)flags;
                }
            }
        }

        alias_registry_destroy(&aliases);
        vault_tree_destroy(&tree);
    }

    if ((mode % 5) == 3) {
        VaultTree tree;
        vault_tree_init(&tree, &arena);

        for (int i = 0; i < 8 && (size_t)(i * 2 + 2) < payload_len; i++) {
            char name[32];
            snprintf(name, sizeof(name), "iter_%d", i);
            VaultNode *n = vault_node_create(name, NODE_TYPE_BLOB);
            if (n) {
                vault_node_set_blob(n, payload + i * 2, 2);
                vault_tree_insert(&tree, "items", n);
            }
        }

        VaultIterator it;
        VaultNode *items = vault_tree_lookup(&tree, "items");
        if (items) {
            vault_iterator_init(&it, items);
            VaultNode *cur;
            int count = 0;
            while ((cur = vault_iterator_next(&it)) != nullptr && count < 50) {
                count++;
            }
        }

        vault_tree_destroy(&tree);
    }

    if ((mode % 5) == 4) {
        uint8_t enc_buf[8192];
        TLVEncoder enc;
        tlv_encoder_init(&enc, enc_buf, sizeof(enc_buf));

        int items = (int)(payload[0] % 8) + 1;
        tlv_begin_sequence(&enc);
        for (int i = 0; i < items && (size_t)(i + 1) < payload_len; i++) {
            if (payload[i + 1] % 3 == 0) {
                tlv_encode_integer(&enc, (int64_t)payload[i + 1]);
            } else if (payload[i + 1] % 3 == 1) {
                tlv_encode_boolean(&enc, payload[i + 1] > 128);
            } else {
                uint8_t small[] = {payload[i + 1]};
                tlv_encode_octet_string(&enc, small, 1);
            }
        }
        tlv_end_sequence(&enc);

        size_t enc_size = tlv_encoder_size(&enc);
        if (enc_size > 0) {
            TLVDecoder dec;
            tlv_decoder_init(&dec, enc_buf, enc_size);
            TLVNode *root = nullptr;
            tlv_decode(&dec, &root);
            tlv_decoder_destroy(&dec);
        }

        tlv_encoder_destroy(&enc);
    }

    arena_destroy(&arena);
    return 0;
}
