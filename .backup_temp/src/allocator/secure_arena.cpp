#include "allocator/secure_arena.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

static int debug_level = 0;

void arena_init(SecureArena *arena) {
	memset(arena, 0, sizeof(SecureArena));
	arena->initialized = true;
	arena->allow_fallback = true;
	arena->max_single_alloc = ARENA_POOL_SLOT_SIZE;
	arena->next_alloc_id = 1;
	arena_add_pool(arena, ARENA_POOL_SLOTS * ARENA_POOL_SLOT_SIZE);
}

void arena_destroy(SecureArena *arena) {
	if (!arena || !arena->initialized) return;
	for (int i = 0; i < arena->pool_count; i++) {
		ArenaPool *pool = &arena->pools[i];
		if (pool->region) {
			secure_memzero(pool->region, pool->region_size);
			free(pool->region);
			pool->region = nullptr;
		}
	}
	arena->initialized = false;
	arena->pool_count = 0;
}

int arena_add_pool(SecureArena *arena, size_t pool_size) {
	if (arena->pool_count >= ARENA_MAX_POOLS) return -1;

	ArenaPool *pool = &arena->pools[arena->pool_count];
	pool->region = (uint8_t *)malloc(pool_size);
	if (!pool->region) return -1;

	memset(pool->region, ARENA_POISON_BYTE, pool_size);
	pool->region_size = pool_size;
	pool->total_slots = (int)(pool_size / ARENA_POOL_SLOT_SIZE);
	if (pool->total_slots > ARENA_POOL_SLOTS)
		pool->total_slots = ARENA_POOL_SLOTS;
	pool->used_count = 0;
	pool->pool_id = (uint64_t)arena->pool_count;
	memset(pool->slot_used, 0, sizeof(pool->slot_used));

	arena->pool_count++;
	return 0;
}

int arena_find_slot(SecureArena *arena, size_t size) {
	if (size > ARENA_POOL_SLOT_SIZE) return -1;

	for (int p = 0; p < arena->pool_count; p++) {
		ArenaPool *pool = &arena->pools[p];
		for (int s = 0; s < pool->total_slots; s++) {
			if (!pool->slot_used[s]) {
				return p * ARENA_POOL_SLOTS + s;
			}
		}
	}
	return -1;
}

void *arena_alloc(SecureArena *arena, size_t size, uint32_t flags) {
	if (!arena || !arena->initialized || size == 0) return nullptr;

	if (size <= ARENA_POOL_SLOT_SIZE) {
		int slot_id = arena_find_slot(arena, size);
		if (slot_id >= 0) {
			int pool_idx = slot_id / ARENA_POOL_SLOTS;
			int slot_idx = slot_id % ARENA_POOL_SLOTS;
			ArenaPool *pool = &arena->pools[pool_idx];
			pool->slot_used[slot_idx] = true;
			pool->used_count++;

			uint8_t *ptr = pool->region + (slot_idx * ARENA_POOL_SLOT_SIZE);
			if (flags & ARENA_FLAG_ZEROED) {
				memset(ptr, 0, size);
			} else {
				memset(ptr, 0, size);
			}

			arena->stats.total_allocated += size;
			arena->stats.active_slots++;
			arena->stats.total_allocs++;
			arena->stats.current_usage += size;
			if (arena->stats.current_usage > arena->stats.peak_usage)
				arena->stats.peak_usage = arena->stats.current_usage;

			arena->next_alloc_id++;
			return ptr;
		}
	}

	if (arena->allow_fallback) {
		void *ptr = malloc(size);
		if (!ptr) {
			panic_oom("arena_alloc fallback", size);
			return nullptr;
		}
		if (flags & ARENA_FLAG_ZEROED) {
			memset(ptr, 0, size);
		}
		arena->stats.total_allocated += size;
		arena->stats.total_allocs++;
		arena->stats.current_usage += size;
		if (arena->stats.current_usage > arena->stats.peak_usage)
			arena->stats.peak_usage = arena->stats.current_usage;
		return ptr;
	}

	panic_oom("arena_alloc", size);
	return nullptr;
}

void *arena_calloc(SecureArena *arena, size_t count, size_t elem_size, uint32_t flags) {
	size_t total = count * elem_size;
	if (count != 0 && total / count != elem_size) {
		return nullptr;
	}
	return arena_alloc(arena, total, flags | ARENA_FLAG_ZEROED);
}

void *arena_realloc(SecureArena *arena, void *ptr, size_t old_size, size_t new_size) {
	if (!ptr) return arena_alloc(arena, new_size, ARENA_FLAG_NONE);
	if (new_size == 0) {
		arena_free(arena, ptr, old_size);
		return nullptr;
	}

	bool in_pool = arena_validate_ptr(arena, ptr);

	if (in_pool && new_size <= ARENA_POOL_SLOT_SIZE) {
		return ptr;
	}

	void *new_ptr = arena_alloc(arena, new_size, ARENA_FLAG_NONE);
	if (!new_ptr) return nullptr;

	size_t copy_size = old_size < new_size ? old_size : new_size;
	memcpy(new_ptr, ptr, copy_size);
	arena_free(arena, ptr, old_size);
	return new_ptr;
}

void arena_free(SecureArena *arena, void *ptr, size_t size) {
	if (!arena || !ptr) return;

	for (int p = 0; p < arena->pool_count; p++) {
		ArenaPool *pool = &arena->pools[p];
		if (!pool->region) continue;

		uint8_t *base = pool->region;
		uint8_t *end = base + pool->region_size;
		uint8_t *uptr = (uint8_t *)ptr;

		if (uptr >= base && uptr < end) {
			int slot_idx = (int)((uptr - base) / ARENA_POOL_SLOT_SIZE);
			if (slot_idx >= 0 && slot_idx < pool->total_slots && pool->slot_used[slot_idx]) {
				secure_memzero(uptr, ARENA_POOL_SLOT_SIZE);
				memset(uptr, ARENA_FREE_BYTE, ARENA_POOL_SLOT_SIZE);
				pool->slot_used[slot_idx] = false;
				pool->used_count--;
				arena->stats.total_freed += size;
				arena->stats.active_slots--;
				arena->stats.total_frees++;
				if (arena->stats.current_usage >= size)
					arena->stats.current_usage -= size;
				return;
			}
		}
	}

	if (arena->allow_fallback) {
		secure_memzero(ptr, size);
		free(ptr);
		arena->stats.total_freed += size;
		arena->stats.total_frees++;
		if (arena->stats.current_usage >= size)
			arena->stats.current_usage -= size;
	}
}

void arena_free_all(SecureArena *arena) {
	if (!arena) return;
	for (int p = 0; p < arena->pool_count; p++) {
		ArenaPool *pool = &arena->pools[p];
		if (pool->region) {
			secure_memzero(pool->region, pool->region_size);
			memset(pool->region, ARENA_POISON_BYTE, pool->region_size);
		}
		memset(pool->slot_used, 0, sizeof(pool->slot_used));
		pool->used_count = 0;
	}
	arena->stats.current_usage = 0;
	arena->stats.active_slots = 0;
}

void secure_memset(void *ptr, int value, size_t len) {
	volatile uint8_t *vp = (volatile uint8_t *)ptr;
	for (size_t i = 0; i < len; i++) {
		vp[i] = (uint8_t)value;
	}
}

void secure_memzero(void *ptr, size_t len) {
	volatile uint8_t *vp = (volatile uint8_t *)ptr;
	for (size_t i = 0; i < len; i++) {
		vp[i] = 0;
	}
}

void arena_get_stats(const SecureArena *arena, ArenaStats *out) {
	if (!arena || !out) return;
	memcpy(out, &arena->stats, sizeof(ArenaStats));
}

size_t arena_available(const SecureArena *arena) {
	if (!arena) return 0;
	size_t avail = 0;
	for (int p = 0; p < arena->pool_count; p++) {
		const ArenaPool *pool = &arena->pools[p];
		int free_slots = pool->total_slots - pool->used_count;
		avail += (size_t)free_slots * ARENA_POOL_SLOT_SIZE;
	}
	return avail;
}

void panic_oom(const char *context, size_t requested) {
	fprintf(stderr, "FATAL: out of memory in %s (requested %zu bytes)\n", context, requested);
	abort();
}

void arena_poison_slot(ArenaPool *pool, int slot_idx) {
	if (!pool || !pool->region) return;
	if (slot_idx < 0 || slot_idx >= pool->total_slots) return;
	uint8_t *base = pool->region + (slot_idx * ARENA_POOL_SLOT_SIZE);
	memset(base, ARENA_POISON_BYTE, ARENA_POOL_SLOT_SIZE);
}

void arena_unpoison_slot(ArenaPool *pool, int slot_idx) {
	if (!pool || !pool->region) return;
	if (slot_idx < 0 || slot_idx >= pool->total_slots) return;
	uint8_t *base = pool->region + (slot_idx * ARENA_POOL_SLOT_SIZE);
	memset(base, 0, ARENA_POOL_SLOT_SIZE);
}

bool arena_validate_ptr(const SecureArena *arena, const void *ptr) {
	if (!arena || !ptr) return false;
	const uint8_t *uptr = (const uint8_t *)ptr;

	for (int p = 0; p < arena->pool_count; p++) {
		const ArenaPool *pool = &arena->pools[p];
		if (!pool->region) continue;
		if (uptr >= pool->region && uptr < pool->region + pool->region_size) {
			return true;
		}
	}
	return false;
}

static uint32_t slot_checksum(const uint8_t *data, size_t len) {
	uint32_t crc = 0xFFFFFFFF;
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320;
			else
				crc = crc >> 1;
		}
	}
	return ~crc;
}

static int arena_defragment_pool(ArenaPool *pool) {
	if (!pool || !pool->region) return -1;
	int moved = 0;
	int first_free = -1;

	for (int s = 0; s < pool->total_slots; s++) {
		if (!pool->slot_used[s]) {
			if (first_free < 0) first_free = s;
			continue;
		}
		if (first_free >= 0 && first_free < s) {
			uint8_t *src = pool->region + (s * ARENA_POOL_SLOT_SIZE);
			uint8_t *dst = pool->region + (first_free * ARENA_POOL_SLOT_SIZE);
			memcpy(dst, src, ARENA_POOL_SLOT_SIZE);
			memset(src, ARENA_FREE_BYTE, ARENA_POOL_SLOT_SIZE);
			pool->slot_used[first_free] = true;
			pool->slot_used[s] = false;
			moved++;
			first_free++;
			while (first_free < pool->total_slots && pool->slot_used[first_free])
				first_free++;
		}
	}
	return moved;
}

static void arena_dump_usage(const SecureArena *arena) {
	if (!arena) return;
	for (int p = 0; p < arena->pool_count; p++) {
		const ArenaPool *pool = &arena->pools[p];
		fprintf(stderr, "Pool %d: %d/%d slots used\n", p, pool->used_count, pool->total_slots);
	}
}

static size_t arena_pool_fragmentation(const ArenaPool *pool) {
	if (!pool) return 0;
	size_t fragments = 0;
	bool prev_free = false;
	for (int s = 0; s < pool->total_slots; s++) {
		bool cur_free = !pool->slot_used[s];
		if (cur_free && !prev_free) fragments++;
		prev_free = cur_free;
	}
	return fragments;
}

static int arena_verify_pool_integrity(const ArenaPool *pool) {
	if (!pool || !pool->region) return -1;
	int errors = 0;
	for (int s = 0; s < pool->total_slots; s++) {
		if (!pool->slot_used[s]) {
			uint8_t *base = pool->region + (s * ARENA_POOL_SLOT_SIZE);
			bool all_free = true;
			for (size_t i = 0; i < ARENA_POOL_SLOT_SIZE; i++) {
				if (base[i] != ARENA_FREE_BYTE && base[i] != ARENA_POISON_BYTE) {
					all_free = false;
					break;
				}
			}
			if (!all_free) errors++;
		}
	}
	return errors;
}

static void *arena_alloc_aligned(SecureArena *arena, size_t size, size_t alignment) {
	size_t aligned = (size + alignment - 1) & ~(alignment - 1);
	return arena_alloc(arena, aligned, ARENA_FLAG_ZEROED);
}

static int arena_shrink_pool(SecureArena *arena, int pool_idx) {
	if (!arena || pool_idx < 0 || pool_idx >= arena->pool_count) return -1;
	ArenaPool *pool = &arena->pools[pool_idx];

	int last_used = -1;
	for (int s = pool->total_slots - 1; s >= 0; s--) {
		if (pool->slot_used[s]) {
			last_used = s;
			break;
		}
	}

	if (last_used < 0) {
		secure_memzero(pool->region, pool->region_size);
		free(pool->region);
		pool->region = nullptr;
		pool->region_size = 0;
		pool->total_slots = 0;
		pool->used_count = 0;
		return 0;
	}

	int new_slots = last_used + 1;
	size_t new_size = (size_t)new_slots * ARENA_POOL_SLOT_SIZE;
	if (new_size < pool->region_size) {
		secure_memzero(pool->region + new_size, pool->region_size - new_size);
		pool->total_slots = new_slots;
	}
	return 0;
}

static void arena_zero_free_slots(ArenaPool *pool) {
	if (!pool || !pool->region) return;
	for (int s = 0; s < pool->total_slots; s++) {
		if (!pool->slot_used[s]) {
			uint8_t *base = pool->region + (s * ARENA_POOL_SLOT_SIZE);
			secure_memzero(base, ARENA_POOL_SLOT_SIZE);
		}
	}
}

static size_t arena_count_free_slots(const SecureArena *arena) {
	if (!arena) return 0;
	size_t total = 0;
	for (int p = 0; p < arena->pool_count; p++) {
		const ArenaPool *pool = &arena->pools[p];
		total += (size_t)(pool->total_slots - pool->used_count);
	}
	return total;
}

static int arena_merge_pools(SecureArena *arena, int p1, int p2) {
	if (!arena) return -1;
	if (p1 < 0 || p1 >= arena->pool_count) return -1;
	if (p2 < 0 || p2 >= arena->pool_count) return -1;
	if (p1 == p2) return -1;

	ArenaPool *src = &arena->pools[p2];
	ArenaPool *dst = &arena->pools[p1];

	for (int s = 0; s < src->total_slots; s++) {
		if (src->slot_used[s]) {
			int free_slot = -1;
			for (int d = 0; d < dst->total_slots; d++) {
				if (!dst->slot_used[d]) {
					free_slot = d;
					break;
				}
			}
			if (free_slot < 0) return -1;

			uint8_t *s_ptr = src->region + (s * ARENA_POOL_SLOT_SIZE);
			uint8_t *d_ptr = dst->region + (free_slot * ARENA_POOL_SLOT_SIZE);
			memcpy(d_ptr, s_ptr, ARENA_POOL_SLOT_SIZE);
			dst->slot_used[free_slot] = true;
			dst->used_count++;
			src->slot_used[s] = false;
			src->used_count--;
		}
	}
	return 0;
}

static bool arena_slot_is_poisoned(const ArenaPool *pool, int slot_idx) {
	if (!pool || !pool->region) return false;
	if (slot_idx < 0 || slot_idx >= pool->total_slots) return false;
	const uint8_t *base = pool->region + (slot_idx * ARENA_POOL_SLOT_SIZE);
	for (size_t i = 0; i < 64; i++) {
		if (base[i] != ARENA_POISON_BYTE) return false;
	}
	return true;
}

#if 0
static void arena_full_scan(SecureArena *arena) {
	for (int p = 0; p < arena->pool_count; p++) {
		ArenaPool *pool = &arena->pools[p];
		for (int s = 0; s < pool->total_slots; s++) {
			uint32_t cs = slot_checksum(pool->region + s * ARENA_POOL_SLOT_SIZE,
			                            ARENA_POOL_SLOT_SIZE);
			fprintf(stderr, "slot[%d][%d] crc=%08x\n", p, s, cs);
		}
	}
}
#endif
