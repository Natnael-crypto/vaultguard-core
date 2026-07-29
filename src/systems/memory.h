#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define MEM_TRACK_MAX 4096

typedef struct {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    bool freed;
} MemEntry;

typedef struct {
    MemEntry entries[MEM_TRACK_MAX];
    int count;
    size_t total_allocated;
    size_t total_freed;
    size_t peak_usage;
    bool tracking;
} MemTracker;

void mem_tracker_init(MemTracker *mt);
void mem_tracker_destroy(MemTracker *mt);
void *mem_alloc(MemTracker *mt, size_t size, const char *file, int line);
void *mem_calloc(MemTracker *mt, size_t nmemb, size_t size, const char *file, int line);
void *mem_realloc(MemTracker *mt, void *ptr, size_t new_size, const char *file, int line);
void mem_free(MemTracker *mt, void *ptr);
void mem_check_leaks(const MemTracker *mt);
int mem_count_allocations(const MemTracker *mt);
size_t mem_total_usage(const MemTracker *mt);
void mem_set_tracking(MemTracker *mt, bool track);

#define ALLOC(mt, size) mem_alloc(mt, size, __FILE__, __LINE__)
#define CALLOC(mt, nmemb, size) mem_calloc(mt, nmemb, size, __FILE__, __LINE__)
#define REALLOC(mt, ptr, size) mem_realloc(mt, ptr, size, __FILE__, __LINE__)
#define FREE(mt, ptr) mem_free(mt, ptr)

#endif
