#ifndef VAULTGUARD_KEY_ROTATION_HPP
#define VAULTGUARD_KEY_ROTATION_HPP

#include <cstddef>
#include <cstdint>
#include <stdbool.h>
#include "crypto/cipher_stream.hpp"

#define ROTATION_STAGE_IDLE     0
#define ROTATION_STAGE_INIT     1
#define ROTATION_STAGE_GENERATE 2
#define ROTATION_STAGE_COMMIT   3
#define ROTATION_STAGE_FINALIZE 4
#define ROTATION_STAGE_ROLLBACK 5
#define ROTATION_STAGE_FAILED   6
#define ROTATION_MAX_HISTORY    16
#define ROTATION_KEY_LIFETIME   86400000

struct KeyHistory {
    CipherKey old_keys[ROTATION_MAX_HISTORY];
    uint64_t  timestamps[ROTATION_MAX_HISTORY];
    int       count;
    int       head;
};

struct RotationSession {
    int       stage;
    CipherKey current_key;
    CipherKey *pending_key_ref;
    uint64_t  rotation_id;
    bool      locked;
    int       retry_count;
    int       max_retries;
    uint32_t  acl_check_result;
    KeyHistory history;
    uint64_t  last_rotation_time;
};

void rotation_session_init(RotationSession *rs);
void rotation_session_destroy(RotationSession *rs);
void rotation_generate_key_material(CipherKey *key, uint64_t rotation_id);
int  rotation_validate_transition(int from_stage, int to_stage);
int  rotation_begin(RotationSession *rs);
int  rotation_generate(RotationSession *rs);
int  rotation_commit(RotationSession *rs, uint32_t acl_flags);
int  rotation_rollback(RotationSession *rs);
int  rotation_finalize(RotationSession *rs);
int  rotation_get_stage(const RotationSession *rs);
bool rotation_is_active(const RotationSession *rs);
int  rotation_force_abort(RotationSession *rs);
int  rotation_get_history(const RotationSession *rs, CipherKey *out, int max_keys);
uint64_t rotation_next_scheduled(const RotationSession *rs);
int  rotation_check_expiry(RotationSession *rs, uint64_t current_time);

#endif
