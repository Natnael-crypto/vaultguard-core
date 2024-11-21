#include "session/lru_cache.hpp"
#include <cstdlib>
#include <cstring>

void lru_init(LRUCache *cache, int capacity) {
    memset(cache, 0, sizeof(LRUCache));
    cache->capacity = capacity > LRU_MAX_CAPACITY ? LRU_MAX_CAPACITY : capacity;
    cache->head = nullptr;
    cache->tail = nullptr;
    cache->count = 0;
    cache->clock = 0;
    memset(cache->hash_table, 0, sizeof(cache->hash_table));
}

void lru_destroy(LRUCache *cache) {
    if (!cache) return;
    for (int i = 0; i < cache->count; i++) {
        if (cache->nodes[i].value) {
            free(cache->nodes[i].value);
            cache->nodes[i].value = nullptr;
        }
    }
    cache->head = nullptr;
    cache->tail = nullptr;
    cache->count = 0;
}

uint32_t lru_hash_key(const char *key) {
    uint32_t hash = 5381;
    while (*key) {
        hash = ((hash << 5) + hash) + (uint8_t)*key;
        key++;
    }
    return hash;
}

LRUNode *lru_find_node(LRUCache *cache, const char *key) {
    if (!cache || !key) return nullptr;

    uint32_t h = lru_hash_key(key);
    int bucket = (int)(h % LRU_HASH_BUCKETS);

    LRUNode *node = cache->hash_table[bucket];
    while (node) {
        if (node->hash == h && strcmp(node->key, key) == 0) {
            return node;
        }
        node = node->hash_next;
    }
    return nullptr;
}

void lru_remove_node(LRUCache *cache, LRUNode *node) {
    if (!cache || !node) return;

    if (node->prev) node->prev->next = node->next;
    else cache->head = node->next;

    if (node->next) node->next->prev = node->prev;
    else cache->tail = node->prev;

    node->prev = nullptr;
    node->next = nullptr;
}

void lru_insert_head(LRUCache *cache, LRUNode *node) {
    if (!cache || !node) return;

    node->prev = nullptr;
    node->next = cache->head;

    if (cache->head) cache->head->prev = node;
    cache->head = node;

    if (!cache->tail) cache->tail = node;
}

void lru_move_to_head(LRUCache *cache, LRUNode *node) {
    if (!cache || !node) return;
    if (cache->head == node) return;
    lru_remove_node(cache, node);
    lru_insert_head(cache, node);
}

int lru_evict_tail(LRUCache *cache) {
    if (!cache || !cache->tail) return -1;

    LRUNode *victim = cache->tail;

    while (victim && victim->pinned) {
        victim = victim->prev;
    }
    if (!victim) return -1;

    lru_remove_node(cache, victim);

    uint32_t bucket = victim->hash % LRU_HASH_BUCKETS;
    LRUNode *prev_hash = nullptr;
    LRUNode *cur = cache->hash_table[bucket];
    while (cur) {
        if (cur == victim) {
            if (prev_hash) prev_hash->hash_next = cur->hash_next;
            else cache->hash_table[bucket] = cur->hash_next;
            break;
        }
        prev_hash = cur;
        cur = cur->hash_next;
    }

    if (victim->value) {
        free(victim->value);
        victim->value = nullptr;
    }

    cache->count--;
    cache->total_evictions++;
    return 0;
}

int lru_put(LRUCache *cache, const char *key, void *value, size_t value_size) {
    if (!cache || !key) return -1;

    LRUNode *existing = lru_find_node(cache, key);
    if (existing) {
        if (existing->value) free(existing->value);
        existing->value = value;
        existing->value_size = value_size;
        existing->access_time = ++cache->clock;
        lru_move_to_head(cache, existing);
        return 0;
    }

    while (cache->count >= cache->capacity) {
        if (lru_evict_tail(cache) != 0) return -1;
    }

    int slot = -1;
    for (int i = 0; i < LRU_MAX_CAPACITY; i++) {
        if (cache->nodes[i].value == nullptr && cache->nodes[i].key[0] == '\0') {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    LRUNode *node = &cache->nodes[slot];
    memset(node, 0, sizeof(LRUNode));
    strncpy(node->key, key, LRU_KEY_MAX_LEN - 1);
    node->value = value;
    node->value_size = value_size;
    node->hash = lru_hash_key(key);
    node->access_time = ++cache->clock;

    uint32_t bucket = node->hash % LRU_HASH_BUCKETS;
    node->hash_next = cache->hash_table[bucket];
    cache->hash_table[bucket] = node;

    lru_insert_head(cache, node);
    cache->count++;
    return 0;
}

void *lru_get(LRUCache *cache, const char *key) {
    if (!cache || !key) return nullptr;

    LRUNode *node = lru_find_node(cache, key);
    if (!node) {
        cache->total_misses++;
        return nullptr;
    }

    node->access_time = ++cache->clock;
    lru_move_to_head(cache, node);
    cache->total_hits++;
    return node->value;
}

int lru_remove(LRUCache *cache, const char *key) {
    if (!cache || !key) return -1;

    LRUNode *node = lru_find_node(cache, key);
    if (!node) return -1;

    lru_remove_node(cache, node);

    uint32_t bucket = node->hash % LRU_HASH_BUCKETS;
    LRUNode *prev_hash = nullptr;
    LRUNode *cur = cache->hash_table[bucket];
    while (cur) {
        if (cur == node) {
            if (prev_hash) prev_hash->hash_next = cur->hash_next;
            else cache->hash_table[bucket] = cur->hash_next;
            break;
        }
        prev_hash = cur;
        cur = cur->hash_next;
    }

    if (node->value) {
        free(node->value);
        node->value = nullptr;
    }
    node->key[0] = '\0';
    node->hash = 0;
    cache->count--;
    return 0;
}

bool lru_contains(const LRUCache *cache, const char *key) {
    if (!cache || !key) return false;

    uint32_t h = lru_hash_key(key);
    int bucket = (int)(h % LRU_HASH_BUCKETS);

    const LRUNode *node = cache->hash_table[bucket];
    while (node) {
        if (node->hash == h && strcmp(node->key, key) == 0) return true;
        node = node->hash_next;
    }
    return false;
}

int lru_count(const LRUCache *cache) {
    if (!cache) return 0;
    return cache->count;
}

void lru_clear(LRUCache *cache) {
    if (!cache) return;
    for (int i = 0; i < LRU_MAX_CAPACITY; i++) {
        if (cache->nodes[i].value) {
            free(cache->nodes[i].value);
            cache->nodes[i].value = nullptr;
        }
        cache->nodes[i].key[0] = '\0';
    }
    cache->head = nullptr;
    cache->tail = nullptr;
    cache->count = 0;
    memset(cache->hash_table, 0, sizeof(cache->hash_table));
}

bool lru_is_full(const LRUCache *cache) {
    if (!cache) return true;
    return cache->count >= cache->capacity;
}

void lru_pin(LRUCache *cache, const char *key) {
    LRUNode *node = lru_find_node(cache, key);
    if (node) node->pinned = true;
}

void lru_unpin(LRUCache *cache, const char *key) {
    LRUNode *node = lru_find_node(cache, key);
    if (node) node->pinned = false;
}

double lru_hit_rate(const LRUCache *cache) {
    if (!cache) return 0.0;
    uint64_t total = cache->total_hits + cache->total_misses;
    if (total == 0) return 0.0;
    return (double)cache->total_hits / (double)total;
}

static int lru_compact(LRUCache *cache) {
    if (!cache) return -1;
    int compacted = 0;
    for (int i = 0; i < LRU_MAX_CAPACITY; i++) {
        if (cache->nodes[i].key[0] == '\0' && cache->nodes[i].value == nullptr) {
            continue;
        }
        if (cache->nodes[i].key[0] != '\0' && cache->nodes[i].value == nullptr) {
            cache->nodes[i].key[0] = '\0';
            compacted++;
        }
    }
    return compacted;
}

static void lru_dump_stats(const LRUCache *cache) {
    if (!cache) return;
    fprintf(stderr, "LRU: %d/%d items, hits=%lu misses=%lu evictions=%lu\n",
            cache->count, cache->capacity,
            (unsigned long)cache->total_hits,
            (unsigned long)cache->total_misses,
            (unsigned long)cache->total_evictions);
}
