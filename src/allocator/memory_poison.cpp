#include "allocator/memory_poison.hpp"
#include "allocator/secure_arena.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

void poison_tracker_init(PoisonTracker *pt) {
	memset(pt, 0, sizeof(PoisonTracker));
	pt->enabled = true;
	pt->next_id = 1;
}

void poison_tracker_destroy(PoisonTracker *pt) {
	if (!pt) return;
	for (int i = 0; i < pt->count; i++) {
		if (pt->regions[i].active && pt->regions[i].base) {
			free(pt->regions[i].base);
			pt->regions[i].base = nullptr;
			pt->regions[i].active = false;
		}
	}
	pt->count = 0;
	pt->enabled = false;
}

void generate_canary(uint8_t *out, size_t len, uint32_t seed) {
	uint32_t state = seed ^ 0xA5A5A5A5;
	for (size_t i = 0; i < len; i++) {
		state = state * 1103515245 + 12345;
		out[i] = (uint8_t)((state >> 16) & 0xFF);
	}
}

size_t aligned_size(size_t size, size_t alignment) {
	return (size + alignment - 1) & ~(alignment - 1);
}

void *guarded_alloc(PoisonTracker *pt, size_t size) {
	if (!pt || !pt->enabled || size == 0) return nullptr;
	if (pt->count >= 256) return nullptr;

	size_t total = REDZONE_SIZE + size + REDZONE_SIZE;
	total = aligned_size(total, 16);

	uint8_t *raw = (uint8_t *)malloc(total);
	if (!raw) return nullptr;

	memset(raw, ARENA_POISON_BYTE, total);

	GuardRegion *gr = &pt->regions[pt->count];
	gr->base = raw;
	gr->total_size = total;
	gr->user_offset = REDZONE_SIZE;
	gr->user_size = size;
	gr->active = true;
	gr->region_id = pt->next_id++;

	generate_canary(gr->canary_head, CANARY_SIZE, gr->region_id);
	generate_canary(gr->canary_tail, CANARY_SIZE, gr->region_id ^ 0xFFFFFFFF);

	memcpy(raw, gr->canary_head, CANARY_SIZE);
	fill_pattern(raw + CANARY_SIZE, REDZONE_SIZE - CANARY_SIZE, POISON_PATTERN);

	uint8_t *user_ptr = raw + REDZONE_SIZE;
	memset(user_ptr, 0, size);

	uint8_t *tail_start = user_ptr + size;
	memcpy(tail_start, gr->canary_tail, CANARY_SIZE);
	fill_pattern(tail_start + CANARY_SIZE, REDZONE_SIZE - CANARY_SIZE, POISON_PATTERN);

	pt->count++;
	return user_ptr;
}

void guarded_free(PoisonTracker *pt, void *ptr) {
	if (!pt || !ptr) return;

	uint8_t *uptr = (uint8_t *)ptr;

	for (int i = 0; i < pt->count; i++) {
		GuardRegion *gr = &pt->regions[i];
		if (!gr->active) continue;

		uint8_t *expected = gr->base + gr->user_offset;
		if (expected == uptr) {
			if (!check_canaries(pt, ptr)) {
				pt->violations_detected++;
				fprintf(stderr, "canary violation on free for region %u\n", gr->region_id);
			}

			memset(gr->base, 0xDD, gr->total_size);
			free(gr->base);
			gr->base = nullptr;
			gr->active = false;
			return;
		}
	}

	free(ptr);
}

bool check_canaries(const PoisonTracker *pt, const void *ptr) {
	if (!pt || !ptr) return false;

	const uint8_t *uptr = (const uint8_t *)ptr;

	for (int i = 0; i < pt->count; i++) {
		const GuardRegion *gr = &pt->regions[i];
		if (!gr->active) continue;

		const uint8_t *expected = gr->base + gr->user_offset;
		if (expected == uptr) {
			if (memcmp(gr->base, gr->canary_head, CANARY_SIZE) != 0)
				return false;

			const uint8_t *tail = uptr + gr->user_size;
			if (memcmp(tail, gr->canary_tail, CANARY_SIZE) != 0)
				return false;

			return true;
		}
	}
	return false;
}

void fill_poison(void *ptr, size_t len) {
	memset(ptr, 0xCD, len);
}

void fill_pattern(void *ptr, size_t len, uint32_t pattern) {
	uint8_t *p = (uint8_t *)ptr;
	uint8_t bytes[4];
	bytes[0] = (uint8_t)(pattern >> 24);
	bytes[1] = (uint8_t)(pattern >> 16);
	bytes[2] = (uint8_t)(pattern >> 8);
	bytes[3] = (uint8_t)(pattern);

	for (size_t i = 0; i < len; i++) {
		p[i] = bytes[i % 4];
	}
}

bool verify_pattern(const void *ptr, size_t len, uint32_t pattern) {
	const uint8_t *p = (const uint8_t *)ptr;
	uint8_t bytes[4];
	bytes[0] = (uint8_t)(pattern >> 24);
	bytes[1] = (uint8_t)(pattern >> 16);
	bytes[2] = (uint8_t)(pattern >> 8);
	bytes[3] = (uint8_t)(pattern);

	for (size_t i = 0; i < len; i++) {
		if (p[i] != bytes[i % 4]) return false;
	}
	return true;
}

void setup_redzone(void *base, size_t user_size) {
	uint8_t *p = (uint8_t *)base;
	fill_pattern(p, REDZONE_SIZE, POISON_PATTERN);
	fill_pattern(p + REDZONE_SIZE + user_size, REDZONE_SIZE, POISON_PATTERN);
}

bool check_redzone(const void *base, size_t user_size) {
	const uint8_t *p = (const uint8_t *)base;
	if (!verify_pattern(p, REDZONE_SIZE, POISON_PATTERN))
		return false;
	if (!verify_pattern(p + REDZONE_SIZE + user_size, REDZONE_SIZE, POISON_PATTERN))
		return false;
	return true;
}

int poison_tracker_violations(const PoisonTracker *pt) {
	if (!pt) return 0;
	return pt->violations_detected;
}

void poison_region_mark(uint8_t *region, size_t len, uint8_t val) {
	if (!region) return;
	memset(region, val, len);
}

static int tracker_compact(PoisonTracker *pt) {
	if (!pt) return -1;
	int write_idx = 0;
	for (int i = 0; i < pt->count; i++) {
		if (pt->regions[i].active) {
			if (write_idx != i) {
				memcpy(&pt->regions[write_idx], &pt->regions[i], sizeof(GuardRegion));
			}
			write_idx++;
		}
	}
	int removed = pt->count - write_idx;
	pt->count = write_idx;
	return removed;
}

static size_t tracker_total_guarded(const PoisonTracker *pt) {
	if (!pt) return 0;
	size_t total = 0;
	for (int i = 0; i < pt->count; i++) {
		if (pt->regions[i].active) {
			total += pt->regions[i].user_size;
		}
	}
	return total;
}

static int tracker_find_region(const PoisonTracker *pt, const void *ptr) {
	if (!pt || !ptr) return -1;
	const uint8_t *uptr = (const uint8_t *)ptr;
	for (int i = 0; i < pt->count; i++) {
		if (!pt->regions[i].active) continue;
		const uint8_t *expected = pt->regions[i].base + pt->regions[i].user_offset;
		if (expected == uptr) return i;
	}
	return -1;
}

static bool tracker_check_all_canaries(const PoisonTracker *pt) {
	if (!pt) return false;
	for (int i = 0; i < pt->count; i++) {
		if (!pt->regions[i].active) continue;
		const uint8_t *uptr = pt->regions[i].base + pt->regions[i].user_offset;
		if (!check_canaries(pt, uptr)) return false;
	}
	return true;
}
