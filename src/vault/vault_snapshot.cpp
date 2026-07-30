#include "vault/vault_snapshot.hpp"
#include "vault/vault_node.hpp"
#include <cstring>
#include <cstdlib>

static int snapshot_serialize_node(const VaultNode *node, uint8_t *out,
                                   size_t max_len, size_t *written) {
    size_t pos = 0;

    if (pos + 4 > max_len) return -1;
    uint32_t name_len = (uint32_t)strlen(node->name);
    memcpy(out + pos, &name_len, 4); pos += 4;
    if (pos + name_len > max_len) return -1;
    memcpy(out + pos, node->name, name_len); pos += name_len;

    if (pos + 12 > max_len) return -1;
    memcpy(out + pos, &node->type, 4); pos += 4;
    memcpy(out + pos, &node->flags, 4); pos += 4;
    memcpy(out + pos, &node->data_size, 4); pos += 4;

    if (node->data_size > 0 && node->type != NODE_TYPE_SUBVAULT) {
        if (pos + node->data_size > max_len) return -1;
        if (node->data.sym_key) {
            memcpy(out + pos, node->data.sym_key, node->data_size);
        }
        pos += node->data_size;
    }

    if (pos + 4 > max_len) return -1;
    uint32_t meta_count = (uint32_t)node->metadata_count;
    memcpy(out + pos, &meta_count, 4); pos += 4;

    for (uint32_t i = 0; i < meta_count; i++) {
        uint32_t mk_len = (uint32_t)strlen(node->metadata[i].key);
        uint32_t mv_len = (uint32_t)strlen(node->metadata[i].value);
        if (pos + 8 + mk_len + mv_len > max_len) return -1;
        memcpy(out + pos, &mk_len, 4); pos += 4;
        memcpy(out + pos, node->metadata[i].key, mk_len); pos += mk_len;
        memcpy(out + pos, &mv_len, 4); pos += 4;
        memcpy(out + pos, node->metadata[i].value, mv_len); pos += mv_len;
    }

    if (node->type == NODE_TYPE_SUBVAULT && node->data.sub_vault) {
        int child_count = node->data.sub_vault->count;
        if (pos + 4 > max_len) return -1;
        uint32_t cc = (uint32_t)child_count;
        memcpy(out + pos, &cc, 4); pos += 4;

        for (int c = 0; c < child_count; c++) {
            VaultNode *child = node->data.sub_vault->entries[c];
            if (child) {
                size_t child_written = 0;
                int rc = snapshot_serialize_node(child, out + pos,
                                                 max_len - pos, &child_written);
                if (rc != 0) return -1;
                pos += child_written;
            }
        }
    }

    *written = pos;
    return 0;
}

static int snapshot_deserialize_node(VaultNode **node_out, const uint8_t *data,
                                     size_t len) {
    size_t pos = 0;
    if (pos + 4 > len) return -1;
    uint32_t name_len;
    memcpy(&name_len, data + pos, 4); pos += 4;
    if (pos + name_len > len) return -1;

    char name[NODE_NAME_MAX];
    memset(name, 0, sizeof(name));
    if (name_len >= NODE_NAME_MAX) name_len = NODE_NAME_MAX - 1;
    memcpy(name, data + pos, name_len); pos += name_len;

    if (pos + 12 > len) return -1;
    uint32_t type, flags, data_size;
    memcpy(&type, data + pos, 4); pos += 4;
    memcpy(&flags, data + pos, 4); pos += 4;
    memcpy(&data_size, data + pos, 4); pos += 4;

    VaultNode *node = vault_node_create(name, type);
    if (!node) return -1;
    node->flags = flags;

    if (data_size > 0 && type != NODE_TYPE_SUBVAULT) {
        if (pos + data_size > len) { vault_node_destroy(node); return -1; }
        uint8_t *buf = (uint8_t *)malloc(data_size);
        if (buf) {
            memcpy(buf, data + pos, data_size);
        }
        pos += data_size;

        if (type == NODE_TYPE_KEY) {
            node->data.sym_key = buf;
            node->data_size = data_size;
        } else if (type == NODE_TYPE_CERT) {
            node->data.cert_der = buf;
            node->data_size = data_size;
        } else if (type == NODE_TYPE_BLOB) {
            node->data.blob_data = buf;
            node->data_size = data_size;
        }
    }

    if (pos + 4 > len) { vault_node_destroy(node); return -1; }
    uint32_t meta_count;
    memcpy(&meta_count, data + pos, 4); pos += 4;

    for (uint32_t i = 0; i < meta_count && i < NODE_MAX_METADATA; i++) {
        if (pos + 4 > len) break;
        uint32_t mk_len;
        memcpy(&mk_len, data + pos, 4); pos += 4;
        if (pos + mk_len > len) break;
        char mkey[64];
        memset(mkey, 0, sizeof(mkey));
        if (mk_len >= sizeof(mkey)) mk_len = sizeof(mkey) - 1;
        memcpy(mkey, data + pos, mk_len); pos += mk_len;

        if (pos + 4 > len) break;
        uint32_t mv_len;
        memcpy(&mv_len, data + pos, 4); pos += 4;
        if (pos + mv_len > len) break;
        char mval[256];
        memset(mval, 0, sizeof(mval));
        if (mv_len >= sizeof(mval)) mv_len = sizeof(mval) - 1;
        memcpy(mval, data + pos, mv_len); pos += mv_len;

        vault_node_set_metadata(node, mkey, mval);
    }

    if (type == NODE_TYPE_SUBVAULT) {
        if (pos + 4 > len) { vault_node_destroy(node); return -1; }
        uint32_t child_count;
        memcpy(&child_count, data + pos, 4); pos += 4;

        if (child_count > 0) {
            vault_node_init_subvault(node, name, (int)child_count);
            for (uint32_t c = 0; c < child_count; c++) {
                VaultNode *child = nullptr;
                int rc = snapshot_deserialize_node(&child, data + pos, len - pos);
                if (rc != 0 || !child) break;
                size_t child_size = 0;
                snapshot_serialize_node(child, nullptr, 0, &child_size);
                pos += child_size;
                vault_node_subvault_add(node, child);
            }
        }
    }

    *node_out = node;
    return 0;
}

static uint32_t snapshot_compute_checksum_val(const uint8_t *data, size_t len) {
    uint32_t sum = 0x56534E50;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
        sum = (sum << 3) | (sum >> 29);
        sum += data[i] * 0x9E3779B9u;
    }
    return sum;
}

int snapshot_compute_checksum(const uint8_t *data, size_t len) {
    if (!data || len == 0) return -1;
    return (int)snapshot_compute_checksum_val(data, len);
}

void snapshot_header_init(SnapshotHeader *header, uint64_t id, const char *name) {
    memset(header, 0, sizeof(SnapshotHeader));
    header->magic = SNAPSHOT_MAGIC;
    header->version = SNAPSHOT_VERSION;
    header->snapshot_id = id;
    header->tag_count = 0;
    header->checksum = 0;
    header->data_offset = sizeof(SnapshotHeader);
    strncpy(header->name, name, SNAPSHOT_MAX_NAME_LEN - 1);
}

int snapshot_header_serialize(const SnapshotHeader *header,
                              uint8_t *out, size_t max_len, size_t *written) {
    if (sizeof(SnapshotHeader) > max_len) return -1;
    memcpy(out, header, sizeof(SnapshotHeader));
    *written = sizeof(SnapshotHeader);
    return 0;
}

int snapshot_header_deserialize(SnapshotHeader *header,
                                const uint8_t *data, size_t len) {
    if (len < sizeof(SnapshotHeader)) return -1;
    memcpy(header, data, sizeof(SnapshotHeader));
    if (header->magic != SNAPSHOT_MAGIC) return -2;
    if (header->version != SNAPSHOT_VERSION) return -3;
    return 0;
}

void snapshot_registry_init(SnapshotRegistry *reg) {
    memset(reg, 0, sizeof(SnapshotRegistry));
    reg->next_id = 1;
    reg->base_time = 0;
    reg->initialized = true;
    reg->total_stored_bytes = 0;
    reg->retention_limit = SNAPSHOT_MAX_SNAPSHOTS;
}

void snapshot_registry_destroy(SnapshotRegistry *reg) {
    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS; i++) {
        if (reg->snapshots[i].data) {
            memset(reg->snapshots[i].data, 0xDD, reg->snapshots[i].data_len);
            free(reg->snapshots[i].data);
            reg->snapshots[i].data = nullptr;
        }
        reg->snapshots[i].valid = false;
        reg->snapshots[i].in_use = false;
    }
    reg->count = 0;
    reg->initialized = false;
}

int snapshot_serialize_tree(const VaultTree *tree, uint8_t *out,
                            size_t max_len, size_t *written) {
    if (!tree || !tree->root) return -1;

    size_t pos = 0;
    if (pos + 4 > max_len) return -1;
    uint32_t version = (uint32_t)tree->tree_version;
    memcpy(out + pos, &version, 4); pos += 4;

    if (pos + 4 > max_len) return -1;
    uint32_t node_count = (uint32_t)tree->node_count;
    memcpy(out + pos, &node_count, 4); pos += 4;

    size_t root_written = 0;
    int rc = snapshot_serialize_node(tree->root, out + pos,
                                     max_len - pos, &root_written);
    if (rc != 0) return -1;
    pos += root_written;

    *written = pos;
    return 0;
}

int snapshot_deserialize_tree(VaultTree *tree, const uint8_t *data, size_t len) {
    size_t pos = 0;
    if (pos + 8 > len) return -1;

    uint32_t version;
    memcpy(&version, data + pos, 4); pos += 4;
    tree->tree_version = version;

    uint32_t node_count;
    memcpy(&node_count, data + pos, 4); pos += 4;

    if (node_count > 0) {
        VaultNode *root = nullptr;
        int rc = snapshot_deserialize_node(&root, data + pos, len - pos);
        if (rc != 0 || !root) return -1;
        tree->root = root;
        tree->node_count = (int)node_count;
    }

    return 0;
}

int snapshot_create(SnapshotRegistry *reg, const VaultTree *tree,
                    const char *name, uint64_t *snapshot_id_out) {
    return snapshot_create_with_tags(reg, tree, name, nullptr, 0, snapshot_id_out);
}

int snapshot_create_with_tags(SnapshotRegistry *reg, const VaultTree *tree,
                              const char *name,
                              const char (*tags)[SNAPSHOT_TAG_MAX_LEN],
                              int tag_count, uint64_t *snapshot_id_out) {
    if (!reg || !tree || !name) return -1;

    int slot = -1;
    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS; i++) {
        if (!reg->snapshots[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -2;

    SnapshotEntry *entry = &reg->snapshots[slot];
    memset(entry, 0, sizeof(SnapshotEntry));

    uint64_t snap_id = reg->next_id++;
    snapshot_header_init(&entry->header, snap_id, name);
    entry->header.tree_version = tree->tree_version;
    entry->header.node_count = (uint32_t)tree->node_count;

    if (tags && tag_count > 0) {
        for (int t = 0; t < tag_count && t < SNAPSHOT_MAX_TAGS; t++) {
            snapshot_tag_add(entry, tags[t]);
        }
    }

    size_t tree_size = 0;
    snapshot_serialize_tree(tree, nullptr, 0, &tree_size);

    uint8_t *buf = (uint8_t *)malloc(tree_size);
    if (!buf) return -3;

    size_t written = 0;
    int rc = snapshot_serialize_tree(tree, buf, tree_size, &written);
    if (rc != 0) {
        free(buf);
        return -4;
    }

    entry->header.data_size = written;
    entry->header.checksum = snapshot_compute_checksum_val(buf, written);
    entry->data = buf;
    entry->data_len = written;
    entry->valid = true;
    entry->in_use = true;

    reg->count++;
    reg->total_stored_bytes += written;

    if (snapshot_id_out) *snapshot_id_out = snap_id;
    return 0;
}

int snapshot_restore(const SnapshotRegistry *reg, uint64_t snapshot_id,
                     VaultTree *tree) {
    if (!reg || !tree) return -1;

    const SnapshotEntry *entry = nullptr;
    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS; i++) {
        if (reg->snapshots[i].in_use && reg->snapshots[i].header.snapshot_id == snapshot_id) {
            entry = &reg->snapshots[i];
            break;
        }
    }
    if (!entry || !entry->data) return -2;

    uint32_t check = snapshot_compute_checksum_val(entry->data, entry->data_len);
    if (check != entry->header.checksum) return -3;

    return snapshot_deserialize_tree(tree, entry->data, entry->data_len);
}

int snapshot_delete(SnapshotRegistry *reg, uint64_t snapshot_id) {
    if (!reg) return -1;

    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS; i++) {
        if (reg->snapshots[i].in_use && reg->snapshots[i].header.snapshot_id == snapshot_id) {
            SnapshotEntry *e = &reg->snapshots[i];
            if (e->data) {
                reg->total_stored_bytes -= e->data_len;
                memset(e->data, 0xDD, e->data_len);
                free(e->data);
                e->data = nullptr;
            }
            e->valid = false;
            e->in_use = false;
            reg->count--;
            return 0;
        }
    }
    return -1;
}

bool snapshot_verify_checksum(const SnapshotEntry *entry) {
    if (!entry || !entry->data) return false;
    uint32_t computed = snapshot_compute_checksum_val(entry->data, entry->data_len);
    return computed == entry->header.checksum;
}

int snapshot_export(const SnapshotRegistry *reg, uint64_t snapshot_id,
                    uint8_t *out, size_t max_len, size_t *written) {
    if (!reg || !out) return -1;

    const SnapshotEntry *entry = nullptr;
    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS; i++) {
        if (reg->snapshots[i].in_use && reg->snapshots[i].header.snapshot_id == snapshot_id) {
            entry = &reg->snapshots[i];
            break;
        }
    }
    if (!entry) return -2;

    size_t pos = 0;
    size_t hdr_written = 0;
    int rc = snapshot_header_serialize(&entry->header, out, max_len, &hdr_written);
    if (rc != 0) return -3;
    pos = hdr_written;

    if (pos + entry->data_len > max_len) return -4;
    memcpy(out + pos, entry->data, entry->data_len);
    pos += entry->data_len;

    *written = pos;
    return 0;
}

int snapshot_import(SnapshotRegistry *reg, const uint8_t *data, size_t len,
                    uint64_t *snapshot_id_out) {
    if (!reg || !data) return -1;

    SnapshotHeader hdr;
    int rc = snapshot_header_deserialize(&hdr, data, len);
    if (rc != 0) return -2;

    int slot = -1;
    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS; i++) {
        if (!reg->snapshots[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -3;

    SnapshotEntry *entry = &reg->snapshots[slot];
    memset(entry, 0, sizeof(SnapshotEntry));

    size_t data_offset = hdr.data_offset;
    size_t data_size = hdr.data_size;
    if (data_offset + data_size > len) return -4;

    uint8_t *buf = (uint8_t *)malloc(data_size);
    if (!buf) return -5;
    memcpy(buf, data + data_offset, data_size);

    entry->header = hdr;
    entry->data = buf;
    entry->data_len = data_size;
    entry->valid = true;
    entry->in_use = true;

    reg->count++;
    reg->total_stored_bytes += data_size;

    if (snapshot_id_out) *snapshot_id_out = hdr.snapshot_id;
    return 0;
}

int snapshot_retention_prune(SnapshotRegistry *reg, int keep_count) {
    if (!reg || keep_count < 0) return -1;

    int pruned = 0;
    while (reg->count > keep_count) {
        uint64_t oldest_id = UINT64_MAX;
        int oldest_idx = -1;
        for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS; i++) {
            if (reg->snapshots[i].in_use && reg->snapshots[i].header.snapshot_id < oldest_id) {
                oldest_id = reg->snapshots[i].header.snapshot_id;
                oldest_idx = i;
            }
        }
        if (oldest_idx < 0) break;

        SnapshotEntry *e = &reg->snapshots[oldest_idx];
        if (e->data) {
            reg->total_stored_bytes -= e->data_len;
            free(e->data);
            e->data = nullptr;
        }
        e->valid = false;
        e->in_use = false;
        reg->count--;
        pruned++;
    }
    return pruned;
}

int snapshot_diff(const SnapshotRegistry *reg, uint64_t base_id,
                  uint64_t target_id, SnapshotDiff *diff) {
    if (!reg || !diff) return -1;

    memset(diff, 0, sizeof(SnapshotDiff));
    diff->base_snapshot_id = base_id;
    diff->target_snapshot_id = target_id;

    return 0;
}

int snapshot_diff_apply(SnapshotDiff *diff, VaultTree *tree) {
    if (!diff || !tree) return -1;
    for (int i = 0; i < diff->op_count; i++) {
        SnapshotDiffOp *op = &diff->ops[i];
        switch (op->op_type) {
            case SNAPSHOT_DIFF_INSERT: {
                VaultNode *node = vault_node_create(op->path, op->node_type);
                if (node) {
                    node->flags = op->node_flags;
                    vault_tree_insert(tree, op->path, node);
                }
                break;
            }
            case SNAPSHOT_DIFF_DELETE:
                vault_tree_delete_force(tree, op->path);
                break;
            case SNAPSHOT_DIFF_MOVE:
                vault_tree_move(tree, op->path, op->target_path);
                break;
            default:
                break;
        }
        op->applied = true;
    }
    return 0;
}

int snapshot_tag_add(SnapshotEntry *entry, const char *tag) {
    if (!entry || !tag) return -1;
    if (entry->header.tag_count >= SNAPSHOT_MAX_TAGS) return -2;
    strncpy(entry->header.tags[entry->header.tag_count], tag,
            SNAPSHOT_TAG_MAX_LEN - 1);
    entry->header.tag_count++;
    return 0;
}

int snapshot_tag_remove(SnapshotEntry *entry, const char *tag) {
    if (!entry || !tag) return -1;
    for (int i = 0; i < entry->header.tag_count; i++) {
        if (strcmp(entry->header.tags[i], tag) == 0) {
            entry->header.tags[i][0] = '\0';
            for (int j = i; j < entry->header.tag_count - 1; j++) {
                strncpy(entry->header.tags[j], entry->header.tags[j + 1],
                        SNAPSHOT_TAG_MAX_LEN - 1);
                entry->header.tags[j][SNAPSHOT_TAG_MAX_LEN - 1] = '\0';
            }
            entry->header.tag_count--;
            return 0;
        }
    }
    return -1;
}

bool snapshot_tag_has(const SnapshotEntry *entry, const char *tag) {
    if (!entry || !tag) return false;
    for (int i = 0; i < entry->header.tag_count; i++) {
        if (strcmp(entry->header.tags[i], tag) == 0) return true;
    }
    return false;
}

int snapshot_list(const SnapshotRegistry *reg, uint64_t *ids,
                  int max_ids, int *count) {
    if (!reg) return -1;
    int n = 0;
    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS && n < max_ids; i++) {
        if (reg->snapshots[i].in_use) {
            ids[n++] = reg->snapshots[i].header.snapshot_id;
        }
    }
    *count = n;
    return 0;
}

int snapshot_find_by_name(const SnapshotRegistry *reg, const char *name,
                          uint64_t *snapshot_id) {
    if (!reg || !name) return -1;
    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS; i++) {
        if (reg->snapshots[i].in_use &&
            strcmp(reg->snapshots[i].header.name, name) == 0) {
            *snapshot_id = reg->snapshots[i].header.snapshot_id;
            return 0;
        }
    }
    return -1;
}

int snapshot_find_by_tag(const SnapshotRegistry *reg, const char *tag,
                         uint64_t *ids, int max_ids, int *count) {
    if (!reg || !tag) return -1;
    int n = 0;
    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS && n < max_ids; i++) {
        if (reg->snapshots[i].in_use &&
            snapshot_tag_has(&reg->snapshots[i], tag)) {
            ids[n++] = reg->snapshots[i].header.snapshot_id;
        }
    }
    *count = n;
    return 0;
}

int snapshot_chunk_write(const uint8_t *data, size_t len,
                         uint8_t *out, size_t max_len, size_t *written) {
    if (!data || !out) return -1;
    size_t total = 0;

    while (total < len) {
        size_t chunk = len - total;
        if (chunk > SNAPSHOT_CHUNK_SIZE) chunk = SNAPSHOT_CHUNK_SIZE;

        if (total + 8 + chunk > max_len) return -2;
        uint32_t chunk_hdr = (uint32_t)chunk;
        memcpy(out + total, &chunk_hdr, 4);
        total += 4;
        memcpy(out + total, data + total - 4, chunk);
        total += chunk;
    }

    uint32_t end_marker = 0;
    if (total + 4 <= max_len) {
        memcpy(out + total, &end_marker, 4);
        total += 4;
    }

    *written = total;
    return 0;
}

int snapshot_chunk_read(const uint8_t *data, size_t data_len,
                        uint8_t *out, size_t out_max, size_t *read) {
    if (!data || !out) return -1;
    size_t pos = 0;
    size_t out_pos = 0;

    while (pos + 4 <= data_len) {
        uint32_t chunk_size;
        memcpy(&chunk_size, data + pos, 4);
        pos += 4;

        if (chunk_size == 0) break;
        if (pos + chunk_size > data_len) return -2;
        if (out_pos + chunk_size > out_max) return -3;

        memcpy(out + out_pos, data + pos, chunk_size);
        pos += chunk_size;
        out_pos += chunk_size;
    }

    *read = out_pos;
    return 0;
}

void snapshot_get_stats(const SnapshotRegistry *reg, SnapshotStats *stats) {
    if (!reg || !stats) return;
    stats->total_snapshots = reg->count;
    stats->active_snapshots = 0;
    stats->total_bytes = reg->total_stored_bytes;
    stats->oldest_snapshot_id = UINT64_MAX;
    stats->newest_snapshot_id = 0;
    stats->avg_nodes_per_snapshot = 0;
    stats->avg_bytes_per_snapshot = 0;

    int total_nodes = 0;
    for (int i = 0; i < SNAPSHOT_MAX_SNAPSHOTS; i++) {
        if (reg->snapshots[i].in_use) {
            stats->active_snapshots++;
            total_nodes += (int)reg->snapshots[i].header.node_count;
            if (reg->snapshots[i].header.snapshot_id < stats->oldest_snapshot_id) {
                stats->oldest_snapshot_id = reg->snapshots[i].header.snapshot_id;
            }
            if (reg->snapshots[i].header.snapshot_id > stats->newest_snapshot_id) {
                stats->newest_snapshot_id = reg->snapshots[i].header.snapshot_id;
            }
        }
    }

    if (stats->active_snapshots > 0) {
        stats->avg_nodes_per_snapshot = total_nodes / stats->active_snapshots;
        stats->avg_bytes_per_snapshot = (int)(reg->total_stored_bytes /
                                              (size_t)stats->active_snapshots);
    }
}
