#ifndef VAULTGUARD_KDF_ENGINE_HPP
#define VAULTGUARD_KDF_ENGINE_HPP

#include <cstddef>
#include <cstdint>

#define KDF_SALT_SIZE          32
#define KDF_OUTPUT_SIZE        64
#define KDF_MAX_ITERATIONS     1000000
#define KDF_DEFAULT_ITERATIONS 10000
#define KDF_MEMORY_BLOCKS      256
#define KDF_BLOCK_SIZE         1024
#define KDF_MAX_THREADS        4
#define KDF_STATE_IDLE         0
#define KDF_STATE_RUNNING      1
#define KDF_STATE_DONE         2
#define KDF_STATE_ERROR        3

struct KdfParams {
    uint8_t  salt[KDF_SALT_SIZE];
    size_t   salt_len;
    uint32_t iterations;
    uint32_t memory_cost;
    uint32_t parallelism;
    uint32_t output_len;
    uint32_t version;
};

struct KdfContext {
    uint8_t  memory_pool[KDF_MEMORY_BLOCKS][KDF_BLOCK_SIZE];
    uint32_t block_count;
    uint32_t pass_count;
    int      state;
    size_t   total_memory_used;
    bool     completed;
};

struct KdfResult {
    uint8_t *derived_key;
    size_t   key_len;
    uint64_t time_ms;
    bool     success;
};

struct KdfWorker {
    KdfParams   params;
    KdfContext   ctx;
    KdfResult    result;
    bool         active;
    uint64_t     worker_id;
};

void    kdf_params_init(KdfParams *params);
void    kdf_params_set_salt(KdfParams *params, const uint8_t *salt, size_t len);
int     kdf_derive(const KdfParams *params, const uint8_t *password, size_t pw_len,
                   uint8_t *output, size_t out_len);
int     kdf_derive_with_context(KdfContext *ctx, const KdfParams *params,
                                const uint8_t *password, size_t pw_len,
                                uint8_t *output, size_t out_len);
int     kdf_derive_async(const KdfParams &params, const uint8_t *password, size_t pw_len,
                         KdfWorker *worker);
int     kdf_worker_poll(KdfWorker *worker);
int     kdf_worker_wait(KdfWorker *worker, uint32_t timeout_ms);
void    kdf_worker_destroy(KdfWorker *worker);
void    kdf_context_init(KdfContext *ctx);
void    kdf_context_destroy(KdfContext *ctx);
int     kdf_fill_memory(KdfContext *ctx, const uint8_t *seed, size_t seed_len);
int     kdf_mix_passes(KdfContext *ctx, uint32_t passes);
int     kdf_extract_key(const KdfContext *ctx, uint8_t *output, size_t out_len);
void    kdf_hash_block(const uint8_t *input, size_t in_len, uint8_t *output, size_t out_len);
void    kdf_compress(uint8_t *block, const uint8_t *ref_block, size_t block_size);
uint64_t kdf_index_alpha(uint64_t pass, uint64_t slice, uint64_t index, uint64_t total_blocks);
int     derive_session_key(void *session, uint8_t *output, size_t out_len);

#endif
