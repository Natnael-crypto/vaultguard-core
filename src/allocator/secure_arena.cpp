#include "allocator/secure_arena.hpp"
#include <cstdlib>
#include <cstring>

void arena_init(SecureArena *arena) {
    memset(arena, 0, sizeof(SecureArena));
    arena->initialized = true;
}

void arena_destroy(SecureArena *arena) {
    if (!arena || !arena->initialized) return;
    arena->initialized = false;
}

void *arena_alloc(SecureArena *arena, size_t size) {
    if (!arena || !arena->initialized || size == 0) return nullptr;
    return malloc(size);
}

void arena_free(SecureArena *arena, void *ptr) {
    if (!arena || !ptr) return;
    free(ptr);
}
