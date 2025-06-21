#include <cassert>
#include <cstdio>
#include <cstring>
#include "allocator/secure_arena.hpp"
#include "allocator/memory_poison.hpp"

int main() {
    SecureArena arena;
    arena_init(&arena);

    void *ptr1 = arena_alloc(&arena, 64, ARENA_FLAG_ZEROED);
    assert(ptr1 != nullptr);
    memset(ptr1, 0x42, 64);

    void *ptr2 = arena_alloc(&arena, 128, ARENA_FLAG_NONE);
    assert(ptr2 != nullptr);

    ArenaStats stats;
    arena_get_stats(&arena, &stats);
    assert(stats.active_slots >= 2);
    assert(stats.total_allocs == 2);

    arena_free(&arena, ptr1, 64);
    arena_free(&arena, ptr2, 128);

    arena_get_stats(&arena, &stats);
    assert(stats.active_slots == 0);

    arena_destroy(&arena);

    PoisonTracker pt;
    poison_tracker_init(&pt);
    void *gptr = guarded_alloc(&pt, 64);
    assert(gptr != nullptr);
    assert(check_canaries(&pt, gptr));
    guarded_free(&pt, gptr);
    poison_tracker_destroy(&pt);

    printf("test_allocator passed\n");
    return 0;
}
