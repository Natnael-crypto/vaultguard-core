#ifndef VAULTGUARD_VAULT_SNAPSHOT_HPP
#define VAULTGUARD_VAULT_SNAPSHOT_HPP

#include <cstddef>
#include <cstdint>
#include "vault/vault_tree.hpp"

#define SNAPSHOT_MAX_SNAPSHOTS   64
#define SNAPSHOT_MAX_NAME_LEN   128
#define SNAPSHOT_MAX_DIFF_OPS   512
#define SNAPSHOT_MAGIC          0x56534E50
#define SNAPSHOT_VERSION        1
#define SNAPSHOT_MAX_TAGS       16
#define SNAPSHOT_TAG_MAX_LEN    64
#define SNAPSHOT_CHUNK_SIZE     4096

#define SNAPSHOT_DIFF_INSERT    0x01
#define SNAPSHOT_DIFF_DELETE    0x02
#define SNAPSHOT_DIFF_MODIFY    0x03
#define SNAPSHOT_DIFF_MOVE      0x04
#define SNAPSHOT_DIFF_RENAME    0x05

#define SNAPSHOT_FLAG_ENCRYPTED  0x01
#define SNAPSHOT_FLAG_COMPRESSED 0x02
#define SNAPSHOT_FLAG_INCREMENTAL 0x04

struct SnapshotDiffOp {
    uint8_t  op_type;
    char     path[512];
    char     target_path[512];
    uint32_t node_type;
    uint32_t node_flags;
    size_t   data_size;
    uint8_t  *data;
    bool     applied;
};

struct SnapshotDiff {
    SnapshotDiffOp  ops[SNAPSHOT_MAX_DIFF_OPS];
    int             op_count;
    uint64_t        base_snapshot_id;
    uint64_t        target_snapshot_id;
    size_t          total_bytes_changed;
    int             nodes_inserted;
    int             nodes_deleted;
    int             nodes_modified;
};

struct SnapshotHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t snapshot_id;
    uint64_t created_at;
    uint64_t tree_version;
    uint32_t node_count;
    uint32_t flags;
    char     name[SNAPSHOT_MAX_NAME_LEN];
    char     tags[SNAPSHOT_MAX_TAGS][SNAPSHOT_TAG_MAX_LEN];
    int      tag_count;
    uint32_t checksum;
    size_t   data_offset;
    size_t   data_size;
};

struct SnapshotEntry {
    SnapshotHeader  header;
    bool            valid;
    bool            in_use;
    uint8_t        *data;
    size_t          data_len;
};

struct SnapshotRegistry {
    SnapshotEntry   snapshots[SNAPSHOT_MAX_SNAPSHOTS];
    int             count;
    uint64_t        next_id;
    uint64_t        base_time;
    bool            initialized;
    size_t          total_stored_bytes;
    int             retention_limit;
};

struct SnapshotStats {
    int      total_snapshots;
    int      active_snapshots;
    uint64_t total_bytes;
    uint64_t oldest_snapshot_id;
    uint64_t newest_snapshot_id;
    int      avg_nodes_per_snapshot;
    double   avg_bytes_per_snapshot;
};

void     snapshot_registry_init(SnapshotRegistry *reg);
void     snapshot_registry_destroy(SnapshotRegistry *reg);
int      snapshot_create(SnapshotRegistry *reg, const VaultTree *tree,
                         const char *name, uint64_t *snapshot_id_out);
int      snapshot_create_with_tags(SnapshotRegistry *reg, const VaultTree *tree,
                                   const char *name,
                                   const char (*tags)[SNAPSHOT_TAG_MAX_LEN],
                                   int tag_count, uint64_t *snapshot_id_out);
int      snapshot_restore(const SnapshotRegistry *reg, uint64_t snapshot_id,
                          VaultTree *tree);
int      snapshot_delete(SnapshotRegistry *reg, uint64_t snapshot_id);
int      snapshot_serialize_tree(const VaultTree *tree, uint8_t *out,
                                 size_t max_len, size_t *written);
int      snapshot_deserialize_tree(VaultTree *tree, const uint8_t *data,
                                   size_t len);
int      snapshot_diff(const SnapshotRegistry *reg, uint64_t base_id,
                       uint64_t target_id, SnapshotDiff *diff);
int      snapshot_diff_apply(SnapshotDiff *diff, VaultTree *tree);
int      snapshot_compute_checksum(const uint8_t *data, size_t len);
bool     snapshot_verify_checksum(const SnapshotEntry *entry);
int      snapshot_export(const SnapshotRegistry *reg, uint64_t snapshot_id,
                         uint8_t *out, size_t max_len, size_t *written);
int      snapshot_import(SnapshotRegistry *reg, const uint8_t *data,
                         size_t len, uint64_t *snapshot_id_out);
int      snapshot_retention_prune(SnapshotRegistry *reg, int keep_count);
int      snapshot_tag_add(SnapshotEntry *entry, const char *tag);
int      snapshot_tag_remove(SnapshotEntry *entry, const char *tag);
bool     snapshot_tag_has(const SnapshotEntry *entry, const char *tag);
int      snapshot_list(const SnapshotRegistry *reg, uint64_t *ids,
                       int max_ids, int *count);
int      snapshot_find_by_name(const SnapshotRegistry *reg, const char *name,
                               uint64_t *snapshot_id);
int      snapshot_find_by_tag(const SnapshotRegistry *reg, const char *tag,
                              uint64_t *ids, int max_ids, int *count);
void     snapshot_header_init(SnapshotHeader *header, uint64_t id,
                              const char *name);
int      snapshot_header_serialize(const SnapshotHeader *header,
                                   uint8_t *out, size_t max_len, size_t *written);
int      snapshot_header_deserialize(SnapshotHeader *header,
                                     const uint8_t *data, size_t len);
int      snapshot_chunk_write(const uint8_t *data, size_t len,
                              uint8_t *out, size_t max_len, size_t *written);
int      snapshot_chunk_read(const uint8_t *data, size_t data_len,
                             uint8_t *out, size_t out_max, size_t *read);
void     snapshot_get_stats(const SnapshotRegistry *reg, SnapshotStats *stats);

#endif
