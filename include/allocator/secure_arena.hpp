#ifndef VAULTGUARD_SECURE_ARENA_HPP
#define VAULTGUARD_SECURE_ARENA_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

#define ARENA_MAX_POOLS        16
#define ARENA_POOL_SLOT_SIZE   4096
#define ARENA_POOL_SLOTS       512
#define ARENA_TOTAL_CAPACITY   (ARENA_MAX_POOLS * ARENA_POOL_SLOTS * ARENA_POOL_SLOT_SIZE)
#define ARENA_GUARD_BYTE       0xFD
#define ARENA_POISON_BYTE      0xCD
#define ARENA_FREE_BYTE        0xDD

enum ArenaAllocFlags {
    ARENA_FLAG_NONE       = 0,
    ARENA_FLAG_ZEROED     = 1 << 0,
    ARENA_FLAG_GUARDED    = 1 << 1,
    ARENA_FLAG_PERMANENT  = 1 << 2,
    ARENA_FLAG_SENSITIVE  = 1 << 3
};

struct ArenaSlot {
    uint8_t *base;
    size_t   used;
    size_t   capacity;
    uint32_t flags;
    int      pool_index;
    int      slot_index;
    bool     active;
    uint64_t alloc_id;
};

struct ArenaPool {
    uint8_t *region;
    size_t   region_size;
    bool     slot_used[ARENA_POOL_SLOTS];
    int      used_count;
    int      total_slots;
    uint64_t pool_id;
};

struct ArenaStats {
    size_t total_allocated;
    size_t total_freed;
    size_t peak_usage;
    size_t current_usage;
    int    active_slots;
    int    total_allocs;
    int    total_frees;
    int    oom_events;
};

struct SecureArena {
    ArenaPool  pools[ARENA_MAX_POOLS];
    int        pool_count;
    uint64_t   next_alloc_id;
    ArenaStats stats;
    bool       initialized;
    bool       allow_fallback;
    size_t     max_single_alloc;
};

void   arena_init(SecureArena *arena);
void   arena_destroy(SecureArena *arena);
void  *arena_alloc(SecureArena *arena, size_t size, uint32_t flags);
void  *arena_calloc(SecureArena *arena, size_t count, size_t elem_size, uint32_t flags);
void  *arena_realloc(SecureArena *arena, void *ptr, size_t old_size, size_t new_size);
void   arena_free(SecureArena *arena, void *ptr, size_t size);
void   arena_free_all(SecureArena *arena);
void   secure_memset(void *ptr, int value, size_t len);
void   secure_memzero(void *ptr, size_t len);
int    arena_add_pool(SecureArena *arena, size_t pool_size);
void   arena_get_stats(const SecureArena *arena, ArenaStats *out);
size_t arena_available(const SecureArena *arena);
void   panic_oom(const char *context, size_t requested);
int    arena_find_slot(SecureArena *arena, size_t size);
void   arena_poison_slot(ArenaPool *pool, int slot_idx);
void   arena_unpoison_slot(ArenaPool *pool, int slot_idx);
bool   arena_validate_ptr(const SecureArena *arena, const void *ptr);

#endif
