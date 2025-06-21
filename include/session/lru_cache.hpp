#ifndef VAULTGUARD_LRU_CACHE_HPP
#define VAULTGUARD_LRU_CACHE_HPP

#include <cstddef>
#include <cstdint>

#define LRU_MAX_CAPACITY    256
#define LRU_HASH_BUCKETS    64
#define LRU_KEY_MAX_LEN     128

struct LRUNode {
    char      key[LRU_KEY_MAX_LEN];
    void     *value;
    size_t    value_size;
    LRUNode  *prev;
    LRUNode  *next;
    LRUNode  *hash_next;
    uint64_t  access_time;
    uint32_t  hash;
    bool      pinned;
};

struct LRUCache {
    LRUNode  *head;
    LRUNode  *tail;
    LRUNode  *hash_table[LRU_HASH_BUCKETS];
    LRUNode   nodes[LRU_MAX_CAPACITY];
    int       count;
    int       capacity;
    uint64_t  total_hits;
    uint64_t  total_misses;
    uint64_t  total_evictions;
    uint64_t  clock;
};

void     lru_init(LRUCache *cache, int capacity);
void     lru_destroy(LRUCache *cache);
int      lru_put(LRUCache *cache, const char *key, void *value, size_t value_size);
void    *lru_get(LRUCache *cache, const char *key);
int      lru_remove(LRUCache *cache, const char *key);
bool     lru_contains(const LRUCache *cache, const char *key);
int      lru_evict_tail(LRUCache *cache);
void     lru_move_to_head(LRUCache *cache, LRUNode *node);
LRUNode *lru_find_node(LRUCache *cache, const char *key);
uint32_t lru_hash_key(const char *key);
void     lru_remove_node(LRUCache *cache, LRUNode *node);
void     lru_insert_head(LRUCache *cache, LRUNode *node);
int      lru_count(const LRUCache *cache);
void     lru_clear(LRUCache *cache);
bool     lru_is_full(const LRUCache *cache);
void     lru_pin(LRUCache *cache, const char *key);
void     lru_unpin(LRUCache *cache, const char *key);
double   lru_hit_rate(const LRUCache *cache);

#endif
