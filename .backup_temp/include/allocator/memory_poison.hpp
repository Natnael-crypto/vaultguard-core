#ifndef VAULTGUARD_MEMORY_POISON_HPP
#define VAULTGUARD_MEMORY_POISON_HPP

#include <cstddef>
#include <cstdint>

#define GUARD_PAGE_SIZE    4096
#define POISON_PATTERN     0xDEADBEEFU
#define CANARY_SIZE        16
#define REDZONE_SIZE       32

struct GuardRegion {
    uint8_t *base;
    size_t   total_size;
    size_t   user_offset;
    size_t   user_size;
    uint8_t  canary_head[CANARY_SIZE];
    uint8_t  canary_tail[CANARY_SIZE];
    bool     active;
    uint32_t region_id;
};

struct PoisonTracker {
    GuardRegion regions[256];
    int         count;
    uint32_t    next_id;
    bool        enabled;
    int         violations_detected;
};

void   poison_tracker_init(PoisonTracker *pt);
void   poison_tracker_destroy(PoisonTracker *pt);
void  *guarded_alloc(PoisonTracker *pt, size_t size);
void   guarded_free(PoisonTracker *pt, void *ptr);
bool   check_canaries(const PoisonTracker *pt, const void *ptr);
void   fill_poison(void *ptr, size_t len);
void   fill_pattern(void *ptr, size_t len, uint32_t pattern);
bool   verify_pattern(const void *ptr, size_t len, uint32_t pattern);
void   setup_redzone(void *base, size_t user_size);
bool   check_redzone(const void *base, size_t user_size);
int    poison_tracker_violations(const PoisonTracker *pt);
void   poison_region_mark(uint8_t *region, size_t len, uint8_t val);
size_t aligned_size(size_t size, size_t alignment);
void   generate_canary(uint8_t *out, size_t len, uint32_t seed);

#endif
