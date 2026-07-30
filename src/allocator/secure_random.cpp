#include "allocator/secure_random.hpp"
#include "crypto/sha256.hpp"
#include <cstring>
#include <cstdlib>

static const uint8_t ZERO_KEY[SECURE_RANDOM_KEY_SIZE] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87,
    0x78, 0x69, 0x5A, 0x4B, 0x3C, 0x2D, 0x1E, 0x0F
};

void secure_random_init(SecureRandomState *state) {
    memset(state, 0, sizeof(SecureRandomState));
    memcpy(state->key, ZERO_KEY, SECURE_RANDOM_KEY_SIZE);
    memset(state->counter, 0x42, SECURE_RANDOM_CTR_SIZE);
    state->pool_len = 0;
    state->bytes_generated = 0;
    state->reseed_counter = 0;
    state->initialized = true;
    state->deterministic = false;
    state->total_requests = 0;
    state->total_bytes = 0;
}

void secure_random_destroy(SecureRandomState *state) {
    secure_random_zeroize(state);
}

void secure_random_zeroize(SecureRandomState *state) {
    memset(state->key, 0, SECURE_RANDOM_KEY_SIZE);
    memset(state->counter, 0, SECURE_RANDOM_CTR_SIZE);
    memset(state->pool, 0, SECURE_RANDOM_POOL_SIZE);
    state->pool_len = 0;
    state->initialized = false;
}

int secure_random_seed(SecureRandomState *state, const uint8_t *entropy,
                       size_t len) {
    if (!state || !entropy || len == 0) return -1;
    if (!state->initialized) return -2;

    for (size_t i = 0; i < len && i < SECURE_RANDOM_KEY_SIZE; i++) {
        state->key[i] ^= entropy[i];
    }

    uint8_t iv[SECURE_RANDOM_CTR_SIZE];
    secure_random_generate(state, iv, SECURE_RANDOM_CTR_SIZE);
    memcpy(state->counter, iv, SECURE_RANDOM_CTR_SIZE);

    return 0;
}

void secure_random_reseed(SecureRandomState *state, const uint8_t *entropy,
                          size_t len) {
    if (!state || !state->initialized) return;

    uint8_t derived_key[SECURE_RANDOM_KEY_SIZE];
    uint8_t derived_iv[SECURE_RANDOM_CTR_SIZE];

    HMACState hmac;
    hmac_sha256_init(&hmac, state->key, SECURE_RANDOM_KEY_SIZE);
    hmac_sha256_update(&hmac, entropy, len);
    hmac_sha256_update(&hmac, state->counter, SECURE_RANDOM_CTR_SIZE);
    HMACResult result;
    hmac_sha256_final(&hmac, &result);
    memcpy(derived_key, result.mac, SECURE_RANDOM_KEY_SIZE);

    hmac_sha256_init(&hmac, state->key, SECURE_RANDOM_KEY_SIZE);
    hmac_sha256_update(&hmac, result.mac, SHA256_DIGEST_SIZE);
    hmac_sha256_update(&hmac, entropy, len);
    hmac_sha256_final(&hmac, &result);
    memcpy(derived_iv, result.mac, SECURE_RANDOM_CTR_SIZE);

    memcpy(state->key, derived_key, SECURE_RANDOM_KEY_SIZE);
    memcpy(state->counter, derived_iv, SECURE_RANDOM_CTR_SIZE);
    state->reseed_counter = 0;

    memset(derived_key, 0, SECURE_RANDOM_KEY_SIZE);
    memset(derived_iv, 0, SECURE_RANDOM_CTR_SIZE);
}

void secure_random_increment_counter(SecureRandomState *state) {
    for (int i = SECURE_RANDOM_CTR_SIZE - 1; i >= 0; i--) {
        state->counter[i]++;
        if (state->counter[i] != 0) break;
    }
}

void secure_random_aes_ctr_step(SecureRandomState *state, uint8_t *block_out) {
    uint8_t local_key[SECURE_RANDOM_KEY_SIZE];
    uint8_t local_ctr[SECURE_RANDOM_CTR_SIZE];
    memcpy(local_key, state->key, SECURE_RANDOM_KEY_SIZE);
    memcpy(local_ctr, state->counter, SECURE_RANDOM_CTR_SIZE);

    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        block_out[i] = local_ctr[i % SECURE_RANDOM_CTR_SIZE] ^
                       local_key[i % SECURE_RANDOM_KEY_SIZE];
    }

    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        state->key[i % SECURE_RANDOM_KEY_SIZE] ^= block_out[i];
    }

    secure_random_increment_counter(state);

    memset(local_key, 0, SECURE_RANDOM_KEY_SIZE);
    memset(local_ctr, 0, SECURE_RANDOM_CTR_SIZE);
}

int secure_random_generate(SecureRandomState *state, uint8_t *out, size_t len) {
    if (!state || !out || len == 0) return -1;
    if (!state->initialized) return -2;
    if (len > SECURE_RANDOM_MAX_REQUEST) return -3;

    state->total_requests++;
    state->total_bytes += len;

    size_t offset = 0;

    while (offset < len) {
        if (state->pool_len > 0 && offset < len) {
            size_t avail = state->pool_len;
            size_t needed = len - offset;
            size_t take = (avail < needed) ? avail : needed;
            memcpy(out + offset, state->pool + (SECURE_RANDOM_POOL_SIZE - state->pool_len), take);
            offset += take;
            state->pool_len -= take;
        }

        if (offset < len) {
            uint8_t block[SHA256_BLOCK_SIZE];
            secure_random_aes_ctr_step(state, block);

            size_t block_take = len - offset;
            if (block_take > SHA256_BLOCK_SIZE) block_take = SHA256_BLOCK_SIZE;
            memcpy(out + offset, block, block_take);
            offset += block_take;

            size_t pool_space = SECURE_RANDOM_POOL_SIZE - state->pool_len;
            size_t pool_fill = (pool_space > SHA256_BLOCK_SIZE) ? SHA256_BLOCK_SIZE : pool_space;
            memcpy(state->pool + state->pool_len, block, pool_fill);
            state->pool_len += pool_fill;

            memset(block, 0, SHA256_BLOCK_SIZE);
        }

        state->bytes_generated += offset;
        state->reseed_counter += (uint32_t)len;

        if (state->reseed_counter >= SECURE_RANDOM_RESEED_INTERVAL) {
            uint8_t reseed_entropy[32];
            secure_random_aes_ctr_step(state, reseed_entropy);
            secure_random_reseed(state, reseed_entropy, sizeof(reseed_entropy));
            memset(reseed_entropy, 0, sizeof(reseed_entropy));
        }
    }

    return 0;
}

int secure_random_generate_u64(SecureRandomState *state, uint64_t *out) {
    return secure_random_generate(state, (uint8_t *)out, 8);
}

int secure_random_generate_u32(SecureRandomState *state, uint32_t *out) {
    return secure_random_generate(state, (uint8_t *)out, 4);
}

int secure_random_pool_add(SecureRandomState *state, const uint8_t *data,
                           size_t len) {
    if (!state || !data) return -1;

    for (size_t i = 0; i < len; i++) {
        size_t pool_idx = state->pool_len % SECURE_RANDOM_POOL_SIZE;
        state->pool[pool_idx] ^= data[i];
        if (state->pool_len < SECURE_RANDOM_POOL_SIZE) {
            state->pool_len++;
        }
    }

    return 0;
}

int secure_random_pool_extract(SecureRandomState *state, uint8_t *out,
                               size_t len) {
    if (!state || !out) return -1;
    if (state->pool_len < len) return -2;

    size_t start = SECURE_RANDOM_POOL_SIZE - state->pool_len;
    memcpy(out, state->pool + start, len);

    for (size_t i = 0; i < len; i++) {
        state->pool[start + i] = 0;
    }
    state->pool_len -= len;

    return 0;
}

uint32_t secure_random_pool_entropy_estimate(const SecureRandomState *state) {
    if (!state) return 0;
    uint32_t estimate = state->pool_len * 8;

    uint8_t zero_count = 0;
    for (size_t i = 0; i < state->pool_len; i++) {
        if (state->pool[i] == 0) zero_count++;
    }
    if (zero_count > state->pool_len / 4) {
        estimate /= 2;
    }

    return estimate;
}

void secure_random_pool_wipe(SecureRandomState *state) {
    if (!state) return;
    memset(state->pool, 0xCD, SECURE_RANDOM_POOL_SIZE);
    memset(state->pool, 0, SECURE_RANDOM_POOL_SIZE);
    state->pool_len = 0;
}

int entropy_source_init(EntropySource *src, const char *name, uint32_t weight) {
    if (!src || !name) return -1;
    strncpy(src->name, name, 63);
    src->weight = weight;
    src->samples_collected = 0;
    src->active = true;
    src->reliable = true;
    return 0;
}

int entropy_collect_rdtsc(uint64_t *out) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    *out = ((uint64_t)hi << 32) | lo;
    return 0;
}

int entropy_collect_rdrand(uint64_t *out) {
    uint8_t ok;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*out), "=qm"(ok));
    return ok ? 0 : -1;
}

int entropy_collect_hash(const uint8_t *data, size_t len, uint64_t *out) {
    if (!data || len < 8) return -1;
    uint64_t h = 0x5A5A5A5A5A5A5A5AULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)data[i] << ((i % 8) * 8);
        h *= 0x100000001B3ULL;
        h ^= h >> 33;
    }
    *out = h;
    return 0;
}

void entropy_mixer(uint8_t *pool, size_t pool_len,
                   const uint8_t *new_data, size_t data_len) {
    for (size_t i = 0; i < data_len; i++) {
        size_t idx = i % pool_len;
        pool[idx] ^= new_data[i];
        pool[idx] = (pool[idx] << 3) | (pool[idx] >> 5);
    }
}

void drbg_init(DRBGContext *ctx) {
    memset(ctx, 0, sizeof(DRBGContext));
    ctx->reseed_interval = SECURE_RANDOM_RESEED_INTERVAL;
    ctx->bytes_since_reseed = 0;
    ctx->initialized = false;
}

void drbg_destroy(DRBGContext *ctx) {
    memset(ctx->V, 0, SECURE_RANDOM_CTR_SIZE);
    memset(ctx->C, 0, SECURE_RANDOM_CTR_SIZE);
    ctx->initialized = false;
}

int drbg_instantiate(DRBGContext *ctx, const uint8_t *entropy,
                     size_t entropy_len, const uint8_t *nonce,
                     size_t nonce_len) {
    if (!ctx || !entropy) return -1;

    memset(ctx->V, 0x01, SECURE_RANDOM_CTR_SIZE);

    uint8_t seed_material[SECURE_RANDOM_CTR_SIZE * 2];
    memset(seed_material, 0, sizeof(seed_material));
    size_t copy_len = (entropy_len < SECURE_RANDOM_CTR_SIZE) ? entropy_len : SECURE_RANDOM_CTR_SIZE;
    memcpy(seed_material, entropy, copy_len);
    if (nonce && nonce_len > 0) {
        copy_len = (nonce_len < SECURE_RANDOM_CTR_SIZE) ? nonce_len : SECURE_RANDOM_CTR_SIZE;
        memcpy(seed_material + SECURE_RANDOM_CTR_SIZE, nonce, copy_len);
    }

    drbg_update(ctx, seed_material, sizeof(seed_material));

    HMACState hmac;
    uint8_t key[SECURE_RANDOM_KEY_SIZE];
    memset(key, 0, SECURE_RANDOM_KEY_SIZE);
    hmac_sha256_init(&hmac, key, SECURE_RANDOM_KEY_SIZE);
    hmac_sha256_update(&hmac, ctx->V, SECURE_RANDOM_CTR_SIZE);
    HMACResult result;
    hmac_sha256_final(&hmac, &result);
    memcpy(ctx->C, result.mac, SECURE_RANDOM_CTR_SIZE);

    ctx->bytes_since_reseed = 0;
    ctx->initialized = true;

    memset(seed_material, 0, sizeof(seed_material));
    memset(key, 0, SECURE_RANDOM_KEY_SIZE);
    return 0;
}

int drbg_generate(DRBGContext *ctx, uint8_t *out, size_t len,
                  const uint8_t *additional, size_t add_len) {
    if (!ctx || !out || !ctx->initialized) return -1;
    if (len > SECURE_RANDOM_MAX_REQUEST) return -2;

    if (ctx->bytes_since_reseed + len > ctx->reseed_interval) {
        return -3;
    }

    size_t offset = 0;
    while (offset < len) {
        for (int i = SECURE_RANDOM_CTR_SIZE - 1; i >= 0; i--) {
            ctx->V[i]++;
            if (ctx->V[i] != 0) break;
        }

        uint8_t block[SECURE_RANDOM_CTR_SIZE];
        memcpy(block, ctx->V, SECURE_RANDOM_CTR_SIZE);

        HMACState hmac;
        uint8_t key[SECURE_RANDOM_KEY_SIZE];
        memset(key, 0, SECURE_RANDOM_KEY_SIZE);
        hmac_sha256_init(&hmac, key, SECURE_RANDOM_KEY_SIZE);
        hmac_sha256_update(&hmac, block, SECURE_RANDOM_CTR_SIZE);
        HMACResult result;
        hmac_sha256_final(&hmac, &result);

        size_t copy = len - offset;
        if (copy > SECURE_RANDOM_CTR_SIZE) copy = SECURE_RANDOM_CTR_SIZE;
        memcpy(out + offset, result.mac, copy);
        offset += copy;

        memset(block, 0, SECURE_RANDOM_CTR_SIZE);
        memset(key, 0, SECURE_RANDOM_KEY_SIZE);
    }

    if (additional && add_len > 0) {
        drbg_update(ctx, additional, add_len);
    }

    ctx->bytes_since_reseed += len;
    return 0;
}

int drbg_reseed(DRBGContext *ctx, const uint8_t *entropy,
                size_t entropy_len) {
    if (!ctx || !entropy) return -1;

    memset(ctx->V, 0, SECURE_RANDOM_CTR_SIZE);
    for (size_t i = 0; i < SECURE_RANDOM_CTR_SIZE; i++) {
        ctx->V[i] = entropy[i % entropy_len];
    }

    uint8_t seed_material[SECURE_RANDOM_CTR_SIZE * 2];
    memcpy(seed_material, ctx->V, SECURE_RANDOM_CTR_SIZE);
    memcpy(seed_material + SECURE_RANDOM_CTR_SIZE, entropy,
           (entropy_len < SECURE_RANDOM_CTR_SIZE) ? entropy_len : SECURE_RANDOM_CTR_SIZE);

    drbg_update(ctx, seed_material, sizeof(seed_material));
    ctx->bytes_since_reseed = 0;

    memset(seed_material, 0, sizeof(seed_material));
    return 0;
}

int drbg_block_encrypt(const uint8_t key[SECURE_RANDOM_KEY_SIZE],
                       uint8_t block[SECURE_RANDOM_CTR_SIZE]) {
    for (int i = 0; i < SECURE_RANDOM_CTR_SIZE; i++) {
        block[i] ^= key[i % SECURE_RANDOM_KEY_SIZE];
    }
    return 0;
}

void drbg_update(DRBGContext *ctx, const uint8_t *provided_data,
                 size_t data_len) {
    uint8_t temp[SECURE_RANDOM_CTR_SIZE * 2];
    size_t offset = 0;

    while (offset < sizeof(temp)) {
        for (int i = SECURE_RANDOM_CTR_SIZE - 1; i >= 0; i--) {
            ctx->V[i]++;
            if (ctx->V[i] != 0) break;
        }

        HMACState hmac;
        uint8_t key[SECURE_RANDOM_KEY_SIZE];
        memset(key, 0, SECURE_RANDOM_KEY_SIZE);
        hmac_sha256_init(&hmac, key, SECURE_RANDOM_KEY_SIZE);
        hmac_sha256_update(&hmac, ctx->V, SECURE_RANDOM_CTR_SIZE);
        HMACResult result;
        hmac_sha256_final(&hmac, &result);

        size_t copy = sizeof(temp) - offset;
        if (copy > SHA256_DIGEST_SIZE) copy = SHA256_DIGEST_SIZE;
        memcpy(temp + offset, result.mac, copy);
        offset += copy;

        memset(key, 0, SECURE_RANDOM_KEY_SIZE);
    }

    memcpy(ctx->V, temp, SECURE_RANDOM_CTR_SIZE);
    memcpy(ctx->C, temp + SECURE_RANDOM_CTR_SIZE, SECURE_RANDOM_CTR_SIZE);

    if (provided_data && data_len > 0) {
        for (size_t i = 0; i < data_len && i < SECURE_RANDOM_CTR_SIZE; i++) {
            ctx->V[i] ^= provided_data[i];
        }
    }

    memset(temp, 0, sizeof(temp));
}

void drbg_burn(DRBGContext *ctx) {
    memset(ctx->V, 0, SECURE_RANDOM_CTR_SIZE);
    memset(ctx->C, 0, SECURE_RANDOM_CTR_SIZE);
    ctx->bytes_since_reseed = 0;
    ctx->initialized = false;
}

void secure_random_get_stats(const SecureRandomState *state,
                             SecureRandomStats *stats) {
    if (!state || !stats) return;
    stats->total_requests = state->total_requests;
    stats->total_bytes_generated = state->total_bytes;
    stats->reseed_count = (uint64_t)(state->reseed_counter /
                                     SECURE_RANDOM_RESEED_INTERVAL);
    stats->current_pool_entropy = secure_random_pool_entropy_estimate(state);
    stats->initialized = state->initialized;
    stats->deterministic = state->deterministic;
}

int secure_random_bytes_since_reseed(const SecureRandomState *state) {
    if (!state) return 0;
    return (int)state->reseed_counter;
}

int secure_random_deterministic_mode(SecureRandomState *state,
                                     const uint8_t *seed, size_t seed_len) {
    if (!state || !seed) return -1;

    secure_random_zeroize(state);

    uint8_t derived[SECURE_RANDOM_KEY_SIZE];
    sha256_hash(seed, seed_len, derived);
    memcpy(state->key, derived, SECURE_RANDOM_KEY_SIZE);

    uint8_t iv_seed[SECURE_RANDOM_CTR_SIZE];
    memset(iv_seed, 0x42, SECURE_RANDOM_CTR_SIZE);
    memcpy(iv_seed, seed, (seed_len < SECURE_RANDOM_CTR_SIZE) ? seed_len : SECURE_RANDOM_CTR_SIZE);
    memcpy(state->counter, iv_seed, SECURE_RANDOM_CTR_SIZE);

    state->initialized = true;
    state->deterministic = true;
    state->bytes_generated = 0;
    state->reseed_counter = 0;
    state->total_requests = 0;
    state->total_bytes = 0;

    memset(derived, 0, SECURE_RANDOM_KEY_SIZE);
    memset(iv_seed, 0, SECURE_RANDOM_CTR_SIZE);
    return 0;
}

uint32_t secure_random_range(SecureRandomState *state, uint32_t min_val,
                             uint32_t max_val) {
    if (min_val >= max_val) return min_val;

    uint32_t range = max_val - min_val;
    uint32_t limit = ~((uint32_t)0) - (~((uint32_t)0) % range);
    uint32_t val;

    do {
        secure_random_generate_u32(state, &val);
    } while (val >= limit);

    return min_val + (val % range);
}

int secure_random_shuffle(void *array, size_t count, size_t elem_size,
                          SecureRandomState *state) {
    if (!array || count < 2 || elem_size == 0 || !state) return -1;

    uint8_t *base = (uint8_t *)array;

    for (size_t i = count - 1; i > 0; i--) {
        uint32_t j = secure_random_range(state, 0, (uint32_t)i);
        if (j != i) {
            uint8_t tmp[256];
            if (elem_size <= 256) {
                memcpy(tmp, base + i * elem_size, elem_size);
                memcpy(base + i * elem_size, base + j * elem_size, elem_size);
                memcpy(base + j * elem_size, tmp, elem_size);
                memset(tmp, 0, elem_size);
            } else {
                uint8_t *a = base + i * elem_size;
                uint8_t *b = base + j * elem_size;
                for (size_t k = 0; k < elem_size; k++) {
                    uint8_t t = a[k];
                    a[k] = b[k];
                    b[k] = t;
                }
            }
        }
    }

    return 0;
}
