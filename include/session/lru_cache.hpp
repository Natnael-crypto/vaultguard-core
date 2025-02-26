#ifndef VAULTGUARD_LRU_CACHE_HPP
#define VAULTGUARD_LRU_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <stdbool.h>

#define LRU_MAX_CAPACITY  1024
#define LRU_KEY_MAX_LEN   128
#define LRU_HASH_BUCKETS  256

struct LRUNode {
    char      key[LRU_KEY_MAX_LEN];
    void     *value;
    size_t    value_size;
    uint32_t  hash;
    uint64_t  access_time;
    bool      pinned;
    LRUNode  *prev;
    LRUNode  *next;
    LRUNode  *hash_next;
};

struct LRUCache {
    LRUNode  nodes[LRU_MAX_CAPACITY];
    LRUNode *head;
    LRUNode *tail;
    int      count;
    int      capacity;
    uint64_t clock;
    LRUNode *hash_table[LRU_HASH_BUCKETS];
    uint64_t total_hits;
    uint64_t total_misses;
    uint64_t total_evictions;
};

void   lru_init(LRUCache *cache, int capacity);
void   lru_destroy(LRUCache *cache);
void  *lru_get(LRUCache *cache, const char *key);
int    lru_put(LRUCache *cache, const char *key, void *value, size_t value_size);
int    lru_remove(LRUCache *cache, const char *key);
bool   lru_contains(const LRUCache *cache, const char *key);
int    lru_count(const LRUCache *cache);
void   lru_clear(LRUCache *cache);
bool   lru_is_full(const LRUCache *cache);
void   lru_pin(LRUCache *cache, const char *key);
void   lru_unpin(LRUCache *cache, const char *key);
double lru_hit_rate(const LRUCache *cache);

#endif
