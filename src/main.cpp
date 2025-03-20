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

int main(int argc, char *argv[]) {
    printf("=== VaultGuard Core Self-Test ===\n\n");
    int failures = 0;
    printf("[arena]");
    failures += test_arena();
    printf(" %s\n", failures ? "FAIL" : "OK");
    printf("\n=== Results: %d failures ===\n", failures);
    return failures > 0 ? 1 : 0;
}
