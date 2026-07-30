#include "session/audit_log.hpp"
#include <cstring>

uint32_t audit_hash_source(const char *source) {
    uint32_t hash = 0x811c9dc5;
    while (*source) {
        hash ^= (uint8_t)*source;
        hash *= 0x01000193;
        source++;
    }
    return hash;
}

void audit_format_timestamp(uint64_t ts, char *out, size_t out_max) {
    uint64_t sec = ts / 1000000;
    uint64_t usec = ts % 1000000;
    int pos = 0;
    uint64_t tmp;

    tmp = sec / 86400;
    uint64_t day = tmp;
    uint64_t year = 1970;
    while (day >= 365) {
        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
            if (day >= 366) { day -= 366; year++; }
            else break;
        } else {
            day -= 365;
            year++;
        }
    }

    uint64_t remaining = sec % 86400;
    uint64_t hours = remaining / 3600;
    remaining %= 3600;
    uint64_t minutes = remaining / 60;
    uint64_t seconds = remaining % 60;

    (void)day;

    if (out_max < 24) return;
    out[pos++] = (char)('0' + (year / 1000) % 10);
    out[pos++] = (char)('0' + (year / 100) % 10);
    out[pos++] = (char)('0' + (year / 10) % 10);
    out[pos++] = (char)('0' + year % 10);
    out[pos++] = '-';
    out[pos++] = (char)('0' + (hours / 10) % 10);
    out[pos++] = (char)('0' + hours % 10);
    out[pos++] = ':';
    out[pos++] = (char)('0' + (minutes / 10) % 10);
    out[pos++] = (char)('0' + minutes % 10);
    out[pos++] = ':';
    out[pos++] = (char)('0' + (seconds / 10) % 10);
    out[pos++] = (char)('0' + seconds % 10);
    out[pos++] = '.';
    out[pos++] = (char)('0' + (usec / 100000) % 10);
    out[pos++] = (char)('0' + (usec / 10000) % 10);
    out[pos++] = (char)('0' + (usec / 1000) % 10);
    out[pos++] = '\0';
}

int audit_entry_compute_checksum(const AuditEntry *entry) {
    uint32_t sum = 0x4e6f7453;
    const uint8_t *p = (const uint8_t *)entry;
    for (size_t i = 0; i < sizeof(AuditEntry) - sizeof(uint32_t); i++) {
        sum ^= p[i];
        sum = (sum << 7) | (sum >> 25);
        sum += p[i] * 2654435761u;
    }
    return (int)sum;
}

bool audit_entry_verify_checksum(const AuditEntry *entry) {
    uint32_t stored = entry->checksum;
    uint32_t computed = (uint32_t)audit_entry_compute_checksum(entry);
    return stored == computed;
}

bool audit_entry_matches(const AuditEntry *entry, const AuditFilter *filter) {
    if (!filter->active) return true;
    if (entry->level < filter->min_level) return false;
    if ((entry->category & filter->category_mask) == 0) return false;
    if (filter->has_source_filter) {
        uint32_t h = audit_hash_source(entry->source);
        if (h != filter->source_hash) return false;
    }
    return true;
}

void audit_log_init(AuditLog *log) {
    log->ring.head = 0;
    log->ring.tail = 0;
    log->ring.count = 0;
    log->ring.capacity = AUDIT_RING_CAPACITY;
    log->ring.total_written = 0;
    log->ring.dropped_count = 0;
    log->filter_count = 0;
    log->next_entry_id = 1;
    log->start_time = 0;
    log->initialized = true;
    log->paused = false;
    log->total_entries = 0;
    log->total_errors = 0;
    log->total_warnings = 0;
}

void audit_log_destroy(AuditLog *log) {
    for (int i = 0; i < log->ring.count; i++) {
        int idx = (log->ring.head + i) % log->ring.capacity;
        memset(&log->ring.entries[idx], 0, sizeof(AuditEntry));
    }
    log->ring.head = 0;
    log->ring.tail = 0;
    log->ring.count = 0;
    log->initialized = false;
}

int audit_ring_push(AuditRingBuffer *ring, const AuditEntry *entry) {
    if (ring->count >= ring->capacity) {
        ring->head = (ring->head + 1) % ring->capacity;
        ring->count--;
        ring->dropped_count++;
    }
    ring->entries[ring->tail] = *entry;
    ring->tail = (ring->tail + 1) % ring->capacity;
    ring->count++;
    ring->total_written++;
    return 0;
}

int audit_ring_pop(AuditRingBuffer *ring, AuditEntry *out) {
    if (ring->count == 0) return -1;
    *out = ring->entries[ring->head];
    ring->head = (ring->head + 1) % ring->capacity;
    ring->count--;
    return 0;
}

int audit_ring_peek(const AuditRingBuffer *ring, int offset, AuditEntry *out) {
    if (offset < 0 || offset >= ring->count) return -1;
    int idx = (ring->head + offset) % ring->capacity;
    *out = ring->entries[idx];
    return 0;
}

bool audit_ring_is_full(const AuditRingBuffer *ring) {
    return ring->count >= ring->capacity;
}

bool audit_ring_is_empty(const AuditRingBuffer *ring) {
    return ring->count == 0;
}

int audit_ring_count(const AuditRingBuffer *ring) {
    return ring->count;
}

void audit_ring_clear(AuditRingBuffer *ring) {
    ring->head = 0;
    ring->tail = 0;
    ring->count = 0;
}

int audit_ring_drain(AuditRingBuffer *ring, AuditEntry *out, int max_entries) {
    int drained = 0;
    while (drained < max_entries && ring->count > 0) {
        out[drained] = ring->entries[ring->head];
        ring->head = (ring->head + 1) % ring->capacity;
        ring->count--;
        drained++;
    }
    return drained;
}

void audit_filter_init(AuditFilter *filter, uint8_t min_level,
                       uint8_t category_mask) {
    filter->min_level = min_level;
    filter->category_mask = category_mask;
    filter->source_hash = 0;
    filter->has_source_filter = false;
    filter->active = true;
}

int audit_filter_add(AuditLog *log, const AuditFilter *filter) {
    if (log->filter_count >= AUDIT_FILTER_MAX) return -1;
    log->filters[log->filter_count++] = *filter;
    return log->filter_count - 1;
}

int audit_filter_remove(AuditLog *log, int index) {
    if (index < 0 || index >= log->filter_count) return -1;
    for (int i = index; i < log->filter_count - 1; i++) {
        log->filters[i] = log->filters[i + 1];
    }
    log->filter_count--;
    return 0;
}

static int audit_write_internal(AuditLog *log, uint8_t level, uint8_t category,
                                const char *source, const char *message,
                                uint16_t session_id) {
    if (!log->initialized) return -1;
    if (log->paused) return -2;

    bool allowed = true;
    for (int i = 0; i < log->filter_count; i++) {
        AuditEntry test;
        memset(&test, 0, sizeof(test));
        test.level = level;
        test.category = category;
        strncpy(test.source, source, AUDIT_MAX_SOURCE_LEN - 1);
        if (!audit_entry_matches(&test, &log->filters[i])) {
            allowed = false;
            break;
        }
    }

    if (!allowed) return -3;

    AuditEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.timestamp = log->start_time + log->total_entries;
    entry.entry_id = log->next_entry_id++;
    entry.level = level;
    entry.category = category;
    entry.session_id = session_id;
    strncpy(entry.source, source, AUDIT_MAX_SOURCE_LEN - 1);
    strncpy(entry.message, message, AUDIT_MAX_MESSAGE_LEN - 1);
    entry.checksum = (uint32_t)audit_entry_compute_checksum(&entry);
    entry.finalized = true;

    audit_ring_push(&log->ring, &entry);

    log->total_entries++;
    if (level == AUDIT_LEVEL_ERROR) log->total_errors++;
    if (level == AUDIT_LEVEL_WARN) log->total_warnings++;

    return 0;
}

int audit_log_write(AuditLog *log, uint8_t level, uint8_t category,
                    const char *source, const char *message,
                    uint16_t session_id) {
    return audit_write_internal(log, level, category, source, message, session_id);
}

int audit_log_write_trace(AuditLog *log, const char *source,
                          const char *message, uint16_t session_id) {
    return audit_write_internal(log, AUDIT_LEVEL_TRACE, AUDIT_CATEGORY_SYSTEM,
                                source, message, session_id);
}

int audit_log_write_debug(AuditLog *log, const char *source,
                          const char *message, uint16_t session_id) {
    return audit_write_internal(log, AUDIT_LEVEL_DEBUG, AUDIT_CATEGORY_SYSTEM,
                                source, message, session_id);
}

int audit_log_write_info(AuditLog *log, const char *source,
                         const char *message, uint16_t session_id) {
    return audit_write_internal(log, AUDIT_LEVEL_INFO, AUDIT_CATEGORY_ACCESS,
                                source, message, session_id);
}

int audit_log_write_warn(AuditLog *log, const char *source,
                         const char *message, uint16_t session_id) {
    return audit_write_internal(log, AUDIT_LEVEL_WARN, AUDIT_CATEGORY_SYSTEM,
                                source, message, session_id);
}

int audit_log_write_error(AuditLog *log, const char *source,
                          const char *message, uint16_t session_id) {
    return audit_write_internal(log, AUDIT_LEVEL_ERROR, AUDIT_CATEGORY_SYSTEM,
                                source, message, session_id);
}

int audit_log_write_critical(AuditLog *log, const char *source,
                             const char *message, uint16_t session_id) {
    return audit_write_internal(log, AUDIT_LEVEL_CRITICAL, AUDIT_CATEGORY_SYSTEM,
                                source, message, session_id);
}

int audit_log_query(const AuditLog *log, uint8_t min_level,
                    uint8_t category_mask, const AuditEntry **results,
                    int max_results) {
    int found = 0;
    for (int i = 0; i < log->ring.count && found < max_results; i++) {
        int idx = (log->ring.head + i) % log->ring.capacity;
        const AuditEntry *e = &log->ring.entries[idx];
        if (e->level >= min_level && (e->category & category_mask)) {
            results[found++] = e;
        }
    }
    return found;
}

int audit_log_query_by_session(const AuditLog *log, uint16_t session_id,
                               const AuditEntry **results, int max_results) {
    int found = 0;
    for (int i = 0; i < log->ring.count && found < max_results; i++) {
        int idx = (log->ring.head + i) % log->ring.capacity;
        const AuditEntry *e = &log->ring.entries[idx];
        if (e->session_id == session_id) {
            results[found++] = e;
        }
    }
    return found;
}

int audit_log_serialize(const AuditLog *log, uint8_t *out,
                        size_t max_len, size_t *written) {
    size_t pos = 0;

    if (pos + 8 > max_len) return -1;
    memcpy(out + pos, &log->total_entries, 8);
    pos += 8;
    memcpy(out + pos, &log->total_errors, 8);
    pos += 8;
    memcpy(out + pos, &log->total_warnings, 8);
    pos += 8;

    uint32_t entry_count = (uint32_t)log->ring.count;
    if (pos + 4 > max_len) return -1;
    memcpy(out + pos, &entry_count, 4);
    pos += 4;

    for (uint32_t i = 0; i < entry_count; i++) {
        int idx = (log->ring.head + i) % log->ring.capacity;
        const AuditEntry *e = &log->ring.entries[idx];
        if (pos + sizeof(AuditEntry) > max_len) return -1;
        memcpy(out + pos, e, sizeof(AuditEntry));
        pos += sizeof(AuditEntry);
    }

    *written = pos;
    return 0;
}

int audit_log_deserialize(AuditLog *log, const uint8_t *data, size_t len) {
    size_t pos = 0;
    if (pos + 28 > len) return -1;

    memcpy(&log->total_entries, data + pos, 8); pos += 8;
    memcpy(&log->total_errors, data + pos, 8); pos += 8;
    memcpy(&log->total_warnings, data + pos, 8); pos += 8;

    uint32_t entry_count;
    memcpy(&entry_count, data + pos, 4); pos += 4;

    for (uint32_t i = 0; i < entry_count; i++) {
        if (pos + sizeof(AuditEntry) > len) return -1;
        AuditEntry e;
        memcpy(&e, data + pos, sizeof(AuditEntry));
        pos += sizeof(AuditEntry);
        audit_ring_push(&log->ring, &e);
    }

    return 0;
}

void audit_log_get_stats(const AuditLog *log, AuditStats *stats) {
    stats->total_entries = log->total_entries;
    stats->total_errors = log->total_errors;
    stats->total_warnings = log->total_warnings;
    stats->dropped_entries = log->ring.dropped_count;
    stats->active_filters = log->filter_count;
    stats->ring_usage = log->ring.count;
    stats->ring_fill_ratio = (double)log->ring.count / (double)log->ring.capacity;

    if (log->ring.count > 0) {
        stats->oldest_timestamp = log->ring.entries[log->ring.head].timestamp;
        int last_idx = (log->ring.tail - 1 + log->ring.capacity) % log->ring.capacity;
        stats->newest_timestamp = log->ring.entries[last_idx].timestamp;
    } else {
        stats->oldest_timestamp = 0;
        stats->newest_timestamp = 0;
    }
}

void audit_log_pause(AuditLog *log) {
    log->paused = true;
}

void audit_log_resume(AuditLog *log) {
    log->paused = false;
}

int audit_log_prune(AuditLog *log, uint64_t before_timestamp) {
    int pruned = 0;
    AuditEntry tmp;
    int new_count = 0;
    AuditEntry buffer[AUDIT_RING_CAPACITY];

    for (int i = 0; i < log->ring.count; i++) {
        int idx = (log->ring.head + i) % log->ring.capacity;
        if (log->ring.entries[idx].timestamp >= before_timestamp) {
            buffer[new_count++] = log->ring.entries[idx];
        } else {
            pruned++;
        }
    }

    log->ring.head = 0;
    log->ring.tail = new_count;
    log->ring.count = new_count;
    for (int i = 0; i < new_count; i++) {
        log->ring.entries[i] = buffer[i];
    }

    (void)tmp;
    return pruned;
}

int audit_log_export_csv(const AuditLog *log, uint8_t *out,
                         size_t max_len, size_t *written) {
    size_t pos = 0;
    const char *header = "id,timestamp,level,category,session,source,message\n";
    size_t hlen = strlen(header);
    if (pos + hlen > max_len) return -1;
    memcpy(out + pos, header, hlen);
    pos += hlen;

    for (int i = 0; i < log->ring.count; i++) {
        int idx = (log->ring.head + i) % log->ring.capacity;
        const AuditEntry *e = &log->ring.entries[idx];

        char line[640];
        int len = snprintf(line, sizeof(line),
                           "%u,%lu,%d,%d,%u,%s,%s\n",
                           e->entry_id, (unsigned long)e->timestamp,
                           e->level, e->category,
                           e->session_id, e->source, e->message);

        if (pos + (size_t)len > max_len) return -1;
        memcpy(out + pos, line, (size_t)len);
        pos += (size_t)len;
    }

    *written = pos;
    return 0;
}
