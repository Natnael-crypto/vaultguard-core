#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "allocator/secure_arena.hpp"
#include "allocator/memory_poison.hpp"
#include "crypto/bigint.hpp"
#include "crypto/cipher_stream.hpp"
#include "crypto/cert_parser.hpp"
#include "crypto/kdf_engine.hpp"
#include "session/key_rotation.hpp"
#include "session/lru_cache.hpp"
#include "session/session_manager.hpp"
#include "tlv/tlv_decoder.hpp"
#include "tlv/tlv_encoder.hpp"
#include "vault/vault_node.hpp"
#include "vault/vault_tree.hpp"
#include "vault/vault_alias.hpp"
#include "vault/acl_engine.hpp"

static int test_arena() {
    SecureArena arena;
    arena_init(&arena);

    void *p1 = arena_alloc(&arena, 128, ARENA_FLAG_ZEROED);
    void *p2 = arena_alloc(&arena, 256, ARENA_FLAG_NONE);
    void *p3 = arena_calloc(&arena, 10, 32, ARENA_FLAG_NONE);

    if (!p1 || !p2 || !p3) {
        printf("  arena alloc failed\n");
        arena_destroy(&arena);
        return 1;
    }

    ArenaStats stats;
    arena_get_stats(&arena, &stats);
    printf("  arena: allocs=%d active=%d peak=%zu\n",
           stats.total_allocs, stats.active_slots, stats.peak_usage);

    arena_free(&arena, p1, 128);
    arena_free(&arena, p2, 256);
    arena_free(&arena, p3, 320);

    arena_destroy(&arena);
    return 0;
}

static int test_bigint() {
    BigInt a, b, result;
    bigint_init(&a);
    bigint_init(&b);
    bigint_init(&result);

    bigint_from_uint64(&a, 12345);
    bigint_from_uint64(&b, 67890);
    bigint_add(&result, &a, &b);

    uint64_t sum = bigint_to_uint64(&result);
    printf("  bigint: 12345 + 67890 = %llu\n", (unsigned long long)sum);

    bigint_multiply(&result, &a, &b);
    uint64_t prod = bigint_to_uint64(&result);
    printf("  bigint: 12345 * 67890 = %llu\n", (unsigned long long)prod);

    BigInt q, r;
    bigint_init(&q);
    bigint_init(&r);
    bigint_divide(&q, &r, &b, &a);
    printf("  bigint: 67890 / 12345 = %llu rem %llu\n",
           (unsigned long long)bigint_to_uint64(&q),
           (unsigned long long)bigint_to_uint64(&r));

    return sum == 80235 && prod == 838102050 ? 0 : 1;
}

static int test_cipher() {
    uint8_t key[CIPHER_KEY_SIZE];
    uint8_t nonce[CIPHER_NONCE_SIZE];
    memset(key, 0x42, CIPHER_KEY_SIZE);
    memset(nonce, 0x13, CIPHER_NONCE_SIZE);

    CipherState cs;
    cipher_init(&cs, key, nonce);

    uint8_t plaintext[] = "Hello, VaultGuard!";
    uint8_t ciphertext[64];
    uint8_t decrypted[64];

    cipher_encrypt_block(&cs, plaintext, ciphertext, sizeof(plaintext));

    cipher_init(&cs, key, nonce);
    cipher_decrypt_block(&cs, ciphertext, decrypted, sizeof(plaintext));

    printf("  cipher: round-trip = '%s'\n", decrypted);
    cipher_destroy(&cs);

    return memcmp(plaintext, decrypted, sizeof(plaintext)) == 0 ? 0 : 1;
}

static int test_tlv() {
    uint8_t buffer[4096];
    TLVEncoder enc;
    tlv_encoder_init(&enc, buffer, sizeof(buffer));

    tlv_begin_sequence(&enc);
    tlv_encode_integer(&enc, 42);
    tlv_encode_utf8_string(&enc, "test-vault");
    tlv_encode_boolean(&enc, true);
    tlv_end_sequence(&enc);

    size_t encoded_size = tlv_encoder_size(&enc);
    printf("  tlv: encoded %zu bytes\n", encoded_size);

    TLVDecoder dec;
    tlv_decoder_init(&dec, buffer, encoded_size);

    TLVNode *root = nullptr;
    int rc = tlv_decode(&dec, &root);
    if (rc == 0 && root) {
        printf("  tlv: decoded %d nodes\n", tlv_node_count(root));
    }

    tlv_decoder_destroy(&dec);
    tlv_encoder_destroy(&enc);
    return 0;
}

static int test_vault_tree() {
    SecureArena arena;
    arena_init(&arena);

    VaultTree tree;
    vault_tree_init(&tree, &arena);

    VaultNode *key1 = vault_node_create("my-key", NODE_TYPE_KEY);
    uint8_t key_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    vault_node_set_key(key1, key_data, sizeof(key_data));
    vault_tree_insert(&tree, "keys", key1);

    VaultNode *cert1 = vault_node_create("server-cert", NODE_TYPE_CERT);
    uint8_t cert_data[] = {0x30, 0x82, 0x01, 0x00};
    vault_node_set_cert(cert1, cert_data, sizeof(cert_data));
    vault_tree_insert(&tree, "certs", cert1);

    printf("  vault: %d nodes in tree\n", vault_tree_count(&tree));

    VaultNode *found = vault_tree_lookup(&tree, "keys/my-key");
    printf("  vault: lookup = %s\n", found ? found->name : "(null)");

    vault_tree_destroy(&tree);
    arena_destroy(&arena);
    return 0;
}

static int test_kdf() {
    KdfParams params;
    kdf_params_init(&params);
    params.iterations = 1000;

    uint8_t salt[] = "test-salt-value";
    kdf_params_set_salt(&params, salt, sizeof(salt) - 1);

    uint8_t output[32];
    int rc = kdf_derive(&params, (const uint8_t *)"password", 8, output, sizeof(output));

    printf("  kdf: derive %s, first bytes = %02x%02x%02x%02x\n",
           rc == 0 ? "OK" : "FAIL",
           output[0], output[1], output[2], output[3]);
    return rc;
}

static int test_session() {
    SessionManager sm;
    session_manager_init(&sm);

    SessionContext *ctx = nullptr;
    session_create(&sm, &ctx);

    printf("  session: created id=%llu\n", (unsigned long long)ctx->session_id);

    uint8_t creds[] = "admin:password123";
    session_authenticate(ctx, creds, sizeof(creds) - 1);
    printf("  session: authenticated=%d perms=0x%x\n",
           ctx->authenticated, ctx->permissions);

    session_manager_destroy(&sm);
    return 0;
}

static int test_acl() {
    ACLRuleset rs;
    acl_ruleset_init(&rs);

    ACLEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.subject_id = 1;
    entry.permissions = ACL_PERM_READ | ACL_PERM_WRITE;
    entry.effect = ACL_EFFECT_ALLOW;
    entry.priority = 10;
    strncpy(entry.resource, "/vault/keys/*", sizeof(entry.resource));
    entry.match_type = ACL_MATCH_WILDCARD;
    acl_add_rule(&rs, &entry);

    int result = acl_check(&rs, 1, "/vault/keys/my-key", ACL_PERM_READ);
    printf("  acl: check read = %s\n", result ? "ALLOW" : "DENY");

    result = acl_check(&rs, 1, "/vault/keys/my-key", ACL_PERM_DELETE);
    printf("  acl: check delete = %s\n", result ? "ALLOW" : "DENY");

    acl_ruleset_destroy(&rs);
    return 0;
}

static int test_key_rotation() {
    RotationSession rs;
    rotation_session_init(&rs);

    rotation_generate_key_material(&rs.current_key, 0);

    int rc = rotation_begin(&rs);
    printf("  rotation: begin = %d, stage = %d\n", rc, rotation_get_stage(&rs));

    rc = rotation_generate(&rs);
    printf("  rotation: generate = %d, stage = %d\n", rc, rotation_get_stage(&rs));

    rc = rotation_commit(&rs, 0x01);
    printf("  rotation: commit = %d, stage = %d\n", rc, rotation_get_stage(&rs));

    rc = rotation_finalize(&rs);
    printf("  rotation: finalize = %d, stage = %d\n", rc, rotation_get_stage(&rs));

    rotation_session_destroy(&rs);
    return 0;
}

int main(int argc, char *argv[]) {
    printf("=== VaultGuard Core Self-Test ===\n\n");

    int failures = 0;

    printf("[arena]\n");
    failures += test_arena();

    printf("[bigint]\n");
    failures += test_bigint();

    printf("[cipher]\n");
    failures += test_cipher();

    printf("[tlv]\n");
    failures += test_tlv();

    printf("[vault]\n");
    failures += test_vault_tree();

    printf("[kdf]\n");
    failures += test_kdf();

    printf("[session]\n");
    failures += test_session();

    printf("[acl]\n");
    failures += test_acl();

    printf("[rotation]\n");
    failures += test_key_rotation();

    printf("\n=== Results: %d failures ===\n", failures);
    return failures > 0 ? 1 : 0;
}
