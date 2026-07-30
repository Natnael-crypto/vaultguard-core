#ifndef VAULTGUARD_SECURE_RANDOM_HPP
#define VAULTGUARD_SECURE_RANDOM_HPP

#include <cstddef>
#include <cstdint>

#define SECURE_RANDOM_SEED_LEN     64
#define SECURE_RANDOM_POOL_SIZE    256
#define SECURE_RANDOM_RESEED_INTERVAL 1048576
#define SECURE_RANDOM_MAX_REQUEST  65536
#define SECURE_RANDOM_ENTROPY_SOURCES 8
#define SECURE_RANDOM_CTR_SIZE     16
#define SECURE_RANDOM_KEY_SIZE     32
#define SECURE_RANDOM_MIN_ENTROPY  128

struct EntropySource {
    const char *name;
    uint32_t    weight;
    uint64_t    samples_collected;
    bool        active;
    bool        reliable;
};

struct SecureRandomState {
    uint8_t  key[SECURE_RANDOM_KEY_SIZE];
    uint8_t  counter[SECURE_RANDOM_CTR_SIZE];
    uint8_t  pool[SECURE_RANDOM_POOL_SIZE];
    size_t   pool_len;
    uint64_t bytes_generated;
    uint32_t reseed_counter;
    bool     initialized;
    bool     deterministic;
    uint64_t total_requests;
    uint64_t total_bytes;
};

struct SecureRandomStats {
    uint64_t total_requests;
    uint64_t total_bytes_generated;
    uint64_t reseed_count;
    uint32_t current_pool_entropy;
    bool     initialized;
    bool     deterministic;
};

struct DRBGContext {
    uint8_t  V[SECURE_RANDOM_CTR_SIZE];
    uint8_t  C[SECURE_RANDOM_CTR_SIZE];
    uint32_t reseed_interval;
    uint64_t bytes_since_reseed;
    bool     initialized;
};

void     secure_random_init(SecureRandomState *state);
void     secure_random_destroy(SecureRandomState *state);
int      secure_random_seed(SecureRandomState *state, const uint8_t *entropy,
                            size_t len);
int      secure_random_generate(SecureRandomState *state, uint8_t *out,
                                size_t len);
int      secure_random_generate_u64(SecureRandomState *state, uint64_t *out);
int      secure_random_generate_u32(SecureRandomState *state, uint32_t *out);
void     secure_random_reseed(SecureRandomState *state, const uint8_t *entropy,
                              size_t len);
void     secure_random_increment_counter(SecureRandomState *state);
void     secure_random_aes_ctr_step(SecureRandomState *state,
                                    uint8_t *block_out);
int      secure_random_pool_add(SecureRandomState *state, const uint8_t *data,
                                size_t len);
int      secure_random_pool_extract(SecureRandomState *state, uint8_t *out,
                                    size_t len);
uint32_t secure_random_pool_entropy_estimate(const SecureRandomState *state);
void     secure_random_pool_wipe(SecureRandomState *state);

int      entropy_source_init(EntropySource *src, const char *name,
                             uint32_t weight);
int      entropy_collect_rdtsc(uint64_t *out);
int      entropy_collect_rdrand(uint64_t *out);
int      entropy_collect_hash(const uint8_t *data, size_t len, uint64_t *out);
void     entropy_mixer(uint8_t *pool, size_t pool_len,
                       const uint8_t *new_data, size_t data_len);

void     drbg_init(DRBGContext *ctx);
void     drbg_destroy(DRBGContext *ctx);
int      drbg_instantiate(DRBGContext *ctx, const uint8_t *entropy,
                          size_t entropy_len, const uint8_t *nonce,
                          size_t nonce_len);
int      drbg_generate(DRBGContext *ctx, uint8_t *out, size_t len,
                       const uint8_t *additional, size_t add_len);
int      drbg_reseed(DRBGContext *ctx, const uint8_t *entropy,
                     size_t entropy_len);
int      drbg_block_encrypt(const uint8_t key[SECURE_RANDOM_KEY_SIZE],
                            uint8_t block[SECURE_RANDOM_CTR_SIZE]);
void     drbg_update(DRBGContext *ctx, const uint8_t *provided_data,
                     size_t data_len);
void     drbg_burn(DRBGContext *ctx);

void     secure_random_get_stats(const SecureRandomState *state,
                                 SecureRandomStats *stats);
int      secure_random_bytes_since_reseed(const SecureRandomState *state);
void     secure_random_zeroize(SecureRandomState *state);
int      secure_random_deterministic_mode(SecureRandomState *state,
                                          const uint8_t *seed, size_t seed_len);
uint32_t secure_random_range(SecureRandomState *state, uint32_t min_val,
                             uint32_t max_val);
int      secure_random_shuffle(void *array, size_t count, size_t elem_size,
                               SecureRandomState *state);

#endif
