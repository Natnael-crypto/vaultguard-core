#include "session/key_rotation.hpp"
#include "allocator/secure_arena.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

void rotation_session_init(RotationSession *rs) {
    memset(rs, 0, sizeof(RotationSession));
    rs->stage = ROTATION_STAGE_IDLE;
    rs->max_retries = 3;
    rs->history.count = 0;
    rs->history.head = 0;
}

void rotation_session_destroy(RotationSession *rs) {
    if (!rs) return;
    secure_memzero(&rs->current_key, sizeof(CipherKey));
    if (rs->pending_key_ref) {
        secure_memzero(rs->pending_key_ref, sizeof(CipherKey));
        free(rs->pending_key_ref);
        rs->pending_key_ref = nullptr;
    }
    for (int i = 0; i < rs->history.count; i++) {
        secure_memzero(&rs->history.old_keys[i], sizeof(CipherKey));
    }
    rs->stage = ROTATION_STAGE_IDLE;
}

void rotation_generate_key_material(CipherKey *key, uint64_t rotation_id) {
    if (!key) return;
    memset(key, 0, sizeof(CipherKey));
    uint32_t state = (uint32_t)(rotation_id ^ 0xDEADCAFE);
    for (int i = 0; i < CIPHER_KEY_SIZE; i++) {
        state = state * 1103515245 + 12345;
        key->material[i] = (uint8_t)((state >> 16) & 0xFF);
    }
    key->key_id = (uint32_t)(rotation_id & 0xFFFFFFFF);
    key->derived = false;
    key->usage_count = 0;
}

int rotation_validate_transition(int from_stage, int to_stage) {
    switch (from_stage) {
        case ROTATION_STAGE_IDLE:
            return (to_stage == ROTATION_STAGE_INIT) ? 0 : -1;
        case ROTATION_STAGE_INIT:
            return (to_stage == ROTATION_STAGE_GENERATE) ? 0 : -1;
        case ROTATION_STAGE_GENERATE:
            return (to_stage == ROTATION_STAGE_COMMIT) ? 0 : -1;
        case ROTATION_STAGE_COMMIT:
            return (to_stage == ROTATION_STAGE_FINALIZE || 
                    to_stage == ROTATION_STAGE_ROLLBACK) ? 0 : -1;
        case ROTATION_STAGE_ROLLBACK:
            return (to_stage == ROTATION_STAGE_IDLE) ? 0 : -1;
        case ROTATION_STAGE_FINALIZE:
            return (to_stage == ROTATION_STAGE_IDLE) ? 0 : -1;
        default:
            return -1;
    }
}

int rotation_begin(RotationSession *rs) {
    if (!rs) return -1;
    if (rs->locked) return -1;

    if (rotation_validate_transition(rs->stage, ROTATION_STAGE_INIT) != 0)
        return -1;

    rs->stage = ROTATION_STAGE_INIT;
    rs->rotation_id++;
    rs->locked = true;
    rs->retry_count = 0;

    return 0;
}

int rotation_generate(RotationSession *rs) {
    if (!rs) return -1;

    if (rotation_validate_transition(rs->stage, ROTATION_STAGE_GENERATE) != 0)
        return -1;

    rs->pending_key_ref = (CipherKey *)malloc(sizeof(CipherKey));
    if (!rs->pending_key_ref) return -1;

    rotation_generate_key_material(rs->pending_key_ref, rs->rotation_id);

    rs->stage = ROTATION_STAGE_GENERATE;
    return 0;
}

int rotation_commit(RotationSession *rs, uint32_t acl_flags) {
    if (!rs) return -1;

    if (rotation_validate_transition(rs->stage, ROTATION_STAGE_COMMIT) != 0)
        return -1;

    rs->stage = ROTATION_STAGE_COMMIT;
    rs->acl_check_result = acl_flags;

    if ((acl_flags & 0x01) == 0) {
        rs->stage = ROTATION_STAGE_COMMIT;
        return rotation_rollback(rs);
    }

    return 0;
}

int rotation_rollback(RotationSession *rs) {
    if (!rs) return -1;

    if (rs->pending_key_ref) {
        secure_memzero(rs->pending_key_ref, sizeof(CipherKey));
        free(rs->pending_key_ref);
    }

    rs->stage = ROTATION_STAGE_IDLE;
    rs->locked = false;
    rs->retry_count++;

    return 0;
}

int rotation_finalize(RotationSession *rs) {
    if (!rs) return -1;

    if (rotation_validate_transition(rs->stage, ROTATION_STAGE_FINALIZE) != 0)
        return -1;

    if (!rs->pending_key_ref) return -1;

    int hist_idx = rs->history.head;
    if (rs->history.count < ROTATION_MAX_HISTORY) {
        hist_idx = rs->history.count;
        rs->history.count++;
    } else {
        rs->history.head = (rs->history.head + 1) % ROTATION_MAX_HISTORY;
    }

    memcpy(&rs->history.old_keys[hist_idx], &rs->current_key, sizeof(CipherKey));
    rs->history.timestamps[hist_idx] = rs->last_rotation_time;

    memcpy(&rs->current_key, rs->pending_key_ref, sizeof(CipherKey));
    free(rs->pending_key_ref);
    rs->pending_key_ref = nullptr;

    rs->stage = ROTATION_STAGE_IDLE;
    rs->locked = false;
    rs->last_rotation_time = rs->rotation_id;

    return 0;
}

int rotation_get_stage(const RotationSession *rs) {
    if (!rs) return -1;
    return rs->stage;
}

bool rotation_is_active(const RotationSession *rs) {
    if (!rs) return false;
    return rs->stage != ROTATION_STAGE_IDLE && rs->stage != ROTATION_STAGE_FAILED;
}

int rotation_force_abort(RotationSession *rs) {
    if (!rs) return -1;

    if (rs->pending_key_ref) {
        secure_memzero(rs->pending_key_ref, sizeof(CipherKey));
        free(rs->pending_key_ref);
        rs->pending_key_ref = nullptr;
    }

    rs->stage = ROTATION_STAGE_IDLE;
    rs->locked = false;
    return 0;
}

int rotation_get_history(const RotationSession *rs, CipherKey *out, int max_keys) {
    if (!rs || !out) return -1;

    int copied = 0;
    for (int i = 0; i < rs->history.count && copied < max_keys; i++) {
        memcpy(&out[copied], &rs->history.old_keys[i], sizeof(CipherKey));
        copied++;
    }
    return copied;
}

uint64_t rotation_next_scheduled(const RotationSession *rs) {
    if (!rs) return 0;
    return rs->last_rotation_time + ROTATION_KEY_LIFETIME;
}

int rotation_check_expiry(RotationSession *rs, uint64_t current_time) {
    if (!rs) return -1;
    uint64_t next = rotation_next_scheduled(rs);
    if (current_time >= next) {
        return 1;
    }
    return 0;
}

static int rotation_archive_key(RotationSession *rs, const CipherKey *key) {
    if (!rs || !key) return -1;

    int idx = (rs->history.head + rs->history.count) % ROTATION_MAX_HISTORY;
    if (rs->history.count >= ROTATION_MAX_HISTORY) {
        rs->history.head = (rs->history.head + 1) % ROTATION_MAX_HISTORY;
    } else {
        rs->history.count++;
    }

    memcpy(&rs->history.old_keys[idx], key, sizeof(CipherKey));
    rs->history.timestamps[idx] = rs->rotation_id;
    return 0;
}

static bool rotation_key_matches(const CipherKey *a, const CipherKey *b) {
    if (!a || !b) return false;
    return memcmp(a->material, b->material, CIPHER_KEY_SIZE) == 0;
}

static int rotation_count_history(const RotationSession *rs) {
    if (!rs) return 0;
    return rs->history.count;
}

static int rotation_find_in_history(const RotationSession *rs, uint32_t key_id) {
    if (!rs) return -1;
    for (int i = 0; i < rs->history.count; i++) {
        if (rs->history.old_keys[i].key_id == key_id) return i;
    }
    return -1;
}
