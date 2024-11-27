#ifndef VAULTGUARD_SECURE_ARENA_HPP
#define VAULTGUARD_SECURE_ARENA_HPP

#include <cstddef>
#include <cstdint>

#define ARENA_MAX_POOLS 16
#define ARENA_POOL_SLOT_SIZE 4096
#define ARENA_POOL_SLOTS 512
#define ARENA_POISON_BYTE 0xCD
#define ARENA_FREE_BYTE 0xDD

struct SecureArena {
    // TODO
};

void arena_init(SecureArena *arena);
void arena_destroy(SecureArena *arena);
void *arena_alloc(SecureArena *arena, size_t size);
void arena_free(SecureArena *arena, void *ptr);

#endif
