#ifndef VAULTGUARD_AUDIT_LOG_HPP
#define VAULTGUARD_AUDIT_LOG_HPP

#include <cstddef>
#include <cstdint>

#define AUDIT_MAX_ENTRIES       4096
#define AUDIT_MAX_MESSAGE_LEN   256
#define AUDIT_MAX_SOURCE_LEN    64
#define AUDIT_RING_CAPACITY     2048
#define AUDIT_FILTER_MAX        32
#define AUDIT_FLUSH_THRESHOLD   512

#define AUDIT_LEVEL_TRACE       0
#define AUDIT_LEVEL_DEBUG       1
#define AUDIT_LEVEL_INFO        2
#define AUDIT_LEVEL_WARN        3
#define AUDIT_LEVEL_ERROR       4
#define AUDIT_LEVEL_CRITICAL    5

#define AUDIT_CATEGORY_ACCESS       0x01
#define AUDIT_CATEGORY_CRYPTO       0x02
#define AUDIT_CATEGORY_SESSION      0x04
#define AUDIT_CATEGORY_VAULT        0x08
#define AUDIT_CATEGORY_SYSTEM       0x10
#define AUDIT_CATEGORY_KEY_ROTATION 0x20
#define AUDIT_CATEGORY_ALL          0x3F

struct AuditEntry {
    uint64_t    timestamp;
    uint32_t    entry_id;
    uint8_t     level;
    uint8_t     category;
    uint16_t    session_id;
    char        source[AUDIT_MAX_SOURCE_LEN];
    char        message[AUDIT_MAX_MESSAGE_LEN];
    uint32_t    checksum;
    bool        finalized;
};

struct AuditFilter {
    uint8_t     min_level;
    uint8_t     category_mask;
    uint32_t    source_hash;
    bool        has_source_filter;
    bool        active;
};

struct AuditRingBuffer {
    AuditEntry  entries[AUDIT_RING_CAPACITY];
    int         head;
    int         tail;
    int         count;
    int         capacity;
    uint64_t    total_written;
    uint64_t    dropped_count;
};

struct AuditLog {
    AuditRingBuffer ring;
    AuditFilter     filters[AUDIT_FILTER_MAX];
    int             filter_count;
    uint32_t        next_entry_id;
    uint64_t        start_time;
    bool            initialized;
    bool            paused;
    uint64_t        total_entries;
    uint64_t        total_errors;
    uint64_t        total_warnings;
};

struct AuditStats {
    uint64_t total_entries;
    uint64_t total_errors;
    uint64_t total_warnings;
    uint64_t dropped_entries;
    int      active_filters;
    int      ring_usage;
    double   ring_fill_ratio;
    uint64_t oldest_timestamp;
    uint64_t newest_timestamp;
};

void     audit_log_init(AuditLog *log);
void     audit_log_destroy(AuditLog *log);
int      audit_log_write(AuditLog *log, uint8_t level, uint8_t category,
                         const char *source, const char *message,
                         uint16_t session_id);
int      audit_log_write_trace(AuditLog *log, const char *source,
                               const char *message, uint16_t session_id);
int      audit_log_write_debug(AuditLog *log, const char *source,
                               const char *message, uint16_t session_id);
int      audit_log_write_info(AuditLog *log, const char *source,
                              const char *message, uint16_t session_id);
int      audit_log_write_warn(AuditLog *log, const char *source,
                              const char *message, uint16_t session_id);
int      audit_log_write_error(AuditLog *log, const char *source,
                               const char *message, uint16_t session_id);
int      audit_log_write_critical(AuditLog *log, const char *source,
                                  const char *message, uint16_t session_id);
int      audit_ring_push(AuditRingBuffer *ring, const AuditEntry *entry);
int      audit_ring_pop(AuditRingBuffer *ring, AuditEntry *out);
int      audit_ring_peek(const AuditRingBuffer *ring, int offset, AuditEntry *out);
bool     audit_ring_is_full(const AuditRingBuffer *ring);
bool     audit_ring_is_empty(const AuditRingBuffer *ring);
int      audit_ring_count(const AuditRingBuffer *ring);
void     audit_ring_clear(AuditRingBuffer *ring);
int      audit_ring_drain(AuditRingBuffer *ring, AuditEntry *out, int max_entries);
void     audit_filter_init(AuditFilter *filter, uint8_t min_level,
                           uint8_t category_mask);
int      audit_filter_add(AuditLog *log, const AuditFilter *filter);
int      audit_filter_remove(AuditLog *log, int index);
bool     audit_entry_matches(const AuditEntry *entry, const AuditFilter *filter);
int      audit_entry_compute_checksum(const AuditEntry *entry);
bool     audit_entry_verify_checksum(const AuditEntry *entry);
int      audit_log_query(const AuditLog *log, uint8_t min_level,
                         uint8_t category_mask, const AuditEntry **results,
                         int max_results);
int      audit_log_query_by_session(const AuditLog *log, uint16_t session_id,
                                    const AuditEntry **results, int max_results);
int      audit_log_serialize(const AuditLog *log, uint8_t *out,
                             size_t max_len, size_t *written);
int      audit_log_deserialize(AuditLog *log, const uint8_t *data, size_t len);
void     audit_log_get_stats(const AuditLog *log, AuditStats *stats);
void     audit_log_pause(AuditLog *log);
void     audit_log_resume(AuditLog *log);
int      audit_log_prune(AuditLog *log, uint64_t before_timestamp);
uint32_t audit_hash_source(const char *source);
void     audit_format_timestamp(uint64_t ts, char *out, size_t out_max);
int      audit_log_export_csv(const AuditLog *log, uint8_t *out,
                              size_t max_len, size_t *written);

#endif
