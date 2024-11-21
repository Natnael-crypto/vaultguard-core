#include "crypto/kdf_engine.hpp"
#include "allocator/secure_arena.hpp"
#include "session/key_rotation.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <thread>
#include <atomic>

void kdf_params_init(KdfParams *params) {
    memset(params, 0, sizeof(KdfParams));
    params->iterations = KDF_DEFAULT_ITERATIONS;
    params->memory_cost = KDF_MEMORY_BLOCKS;
    params->parallelism = 1;
    params->output_len = KDF_OUTPUT_SIZE;
    params->version = 1;
}

void kdf_params_set_salt(KdfParams *params, const uint8_t *salt, size_t len) {
    if (!params || !salt) return;
    size_t copy_len = len < KDF_SALT_SIZE ? len : KDF_SALT_SIZE;
    memcpy(params->salt, salt, copy_len);
    params->salt_len = copy_len;
}

void kdf_hash_block(const uint8_t *input, size_t in_len, uint8_t *output, size_t out_len) {
    if (!input || !output) return;

    uint32_t state[16];
    memset(state, 0, sizeof(state));

    for (size_t i = 0; i < in_len; i++) {
        state[i % 16] ^= ((uint32_t)input[i]) << ((i % 4) * 8);
    }

    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 16; i++) {
            state[i] += state[(i + 1) % 16];
            state[i] = (state[i] << 13) | (state[i] >> 19);
            state[i] ^= state[(i + 5) % 16];
        }
    }

    for (size_t i = 0; i < out_len; i++) {
        output[i] = (uint8_t)((state[i % 16] >> ((i % 4) * 8)) & 0xFF);
    }
}

void kdf_compress(uint8_t *block, const uint8_t *ref_block, size_t block_size) {
    if (!block || !ref_block) return;

    for (size_t i = 0; i < block_size; i++) {
        block[i] ^= ref_block[i];
    }

    for (size_t i = 0; i < block_size - 1; i++) {
        block[i] = (uint8_t)((block[i] << 3) | (block[i] >> 5));
        block[i] ^= block[i + 1];
    }
    block[block_size - 1] = (uint8_t)((block[block_size - 1] << 3) | (block[block_size - 1] >> 5));
}

uint64_t kdf_index_alpha(uint64_t pass, uint64_t slice, uint64_t index, uint64_t total_blocks) {
    uint64_t j1 = pass * total_blocks + slice * (total_blocks / 4) + index;
    uint64_t j2 = j1 * j1;
    return j2 % total_blocks;
}

void kdf_context_init(KdfContext *ctx) {
    memset(ctx, 0, sizeof(KdfContext));
    ctx->state = KDF_STATE_IDLE;
}

void kdf_context_destroy(KdfContext *ctx) {
    if (!ctx) return;
    secure_memzero(ctx->memory_pool, sizeof(ctx->memory_pool));
    ctx->state = KDF_STATE_IDLE;
}

int kdf_fill_memory(KdfContext *ctx, const uint8_t *seed, size_t seed_len) {
    if (!ctx || !seed) return -1;

    kdf_hash_block(seed, seed_len, ctx->memory_pool[0], KDF_BLOCK_SIZE);

    for (uint32_t i = 1; i < KDF_MEMORY_BLOCKS; i++) {
        kdf_hash_block(ctx->memory_pool[i - 1], KDF_BLOCK_SIZE,
                       ctx->memory_pool[i], KDF_BLOCK_SIZE);
    }

    ctx->block_count = KDF_MEMORY_BLOCKS;
    return 0;
}

int kdf_mix_passes(KdfContext *ctx, uint32_t passes) {
    if (!ctx) return -1;

    for (uint32_t p = 0; p < passes; p++) {
        for (uint32_t i = 0; i < ctx->block_count; i++) {
            uint64_t ref_idx = kdf_index_alpha(p, i / (ctx->block_count / 4),
                                               i, ctx->block_count);
            kdf_compress(ctx->memory_pool[i], ctx->memory_pool[ref_idx], KDF_BLOCK_SIZE);
        }
        ctx->pass_count++;
    }

    return 0;
}

int kdf_extract_key(const KdfContext *ctx, uint8_t *output, size_t out_len) {
    if (!ctx || !output) return -1;

    uint8_t final_block[KDF_BLOCK_SIZE];
    memset(final_block, 0, KDF_BLOCK_SIZE);

    for (uint32_t i = 0; i < ctx->block_count; i++) {
        for (size_t j = 0; j < KDF_BLOCK_SIZE; j++) {
            final_block[j] ^= ctx->memory_pool[i][j];
        }
    }

    kdf_hash_block(final_block, KDF_BLOCK_SIZE, output, out_len);
    secure_memzero(final_block, KDF_BLOCK_SIZE);
    return 0;
}

int kdf_derive(const KdfParams *params, const uint8_t *password, size_t pw_len,
               uint8_t *output, size_t out_len) {
    if (!params || !password || !output) return -1;

    KdfContext ctx;
    kdf_context_init(&ctx);

    uint8_t seed[KDF_SALT_SIZE + 256];
    size_t seed_len = 0;

    memcpy(seed, params->salt, params->salt_len);
    seed_len += params->salt_len;

    size_t pw_copy = pw_len < 256 ? pw_len : 256;
    memcpy(seed + seed_len, password, pw_copy);
    seed_len += pw_copy;

    if (kdf_fill_memory(&ctx, seed, seed_len) != 0) {
        kdf_context_destroy(&ctx);
        return -1;
    }

    uint32_t passes = params->iterations / KDF_MEMORY_BLOCKS;
    if (passes < 1) passes = 1;
    if (passes > 100) passes = 100;

    if (kdf_mix_passes(&ctx, passes) != 0) {
        kdf_context_destroy(&ctx);
        return -1;
    }

    int rc = kdf_extract_key(&ctx, output, out_len);
    kdf_context_destroy(&ctx);
    secure_memzero(seed, sizeof(seed));
    return rc;
}

int kdf_derive_with_context(KdfContext *ctx, const KdfParams *params,
                            const uint8_t *password, size_t pw_len,
                            uint8_t *output, size_t out_len) {
    if (!ctx || !params || !password || !output) return -1;

    kdf_context_init(ctx);

    uint8_t seed[KDF_SALT_SIZE + 256];
    size_t seed_len = 0;
    memcpy(seed, params->salt, params->salt_len);
    seed_len += params->salt_len;
    size_t pw_copy = pw_len < 256 ? pw_len : 256;
    memcpy(seed + seed_len, password, pw_copy);
    seed_len += pw_copy;

    kdf_fill_memory(ctx, seed, seed_len);

    uint32_t passes = params->iterations / KDF_MEMORY_BLOCKS;
    if (passes < 1) passes = 1;
    if (passes > 100) passes = 100;
    kdf_mix_passes(ctx, passes);

    int rc = kdf_extract_key(ctx, output, out_len);
    secure_memzero(seed, sizeof(seed));
    return rc;
}

static void kdf_worker_thread_fn(KdfWorker *worker, const KdfParams &params) {
    uint8_t output[KDF_OUTPUT_SIZE];
    uint8_t dummy_password[] = "worker_derivation";

    int rc = kdf_derive(&params, dummy_password, sizeof(dummy_password) - 1,
                        output, params.output_len);

    if (rc == 0) {
        worker->result.derived_key = (uint8_t *)malloc(params.output_len);
        if (worker->result.derived_key) {
            memcpy(worker->result.derived_key, output, params.output_len);
            worker->result.key_len = params.output_len;
            worker->result.success = true;
        }
    }

    worker->ctx.state = KDF_STATE_DONE;
    worker->ctx.completed = true;
    worker->active = false;
    secure_memzero(output, sizeof(output));
}

int kdf_derive_async(const KdfParams &params, const uint8_t *password, size_t pw_len,
                     KdfWorker *worker) {
    if (!worker) return -1;

    memset(worker, 0, sizeof(KdfWorker));
    worker->active = true;
    worker->ctx.state = KDF_STATE_RUNNING;

    std::thread worker_thread(kdf_worker_thread_fn, worker, std::ref(params));
    worker_thread.detach();

    return 0;
}

int kdf_worker_poll(KdfWorker *worker) {
    if (!worker) return -1;
    if (worker->ctx.completed) return 1;
    if (!worker->active) return -1;
    return 0;
}

int kdf_worker_wait(KdfWorker *worker, uint32_t timeout_ms) {
    if (!worker) return -1;

    uint32_t elapsed = 0;
    while (!worker->ctx.completed && elapsed < timeout_ms) {
        struct timespec ts = {0, 1000000};
        elapsed++;
    }

    return worker->ctx.completed ? 0 : -1;
}

void kdf_worker_destroy(KdfWorker *worker) {
    if (!worker) return;
    if (worker->result.derived_key) {
        secure_memzero(worker->result.derived_key, worker->result.key_len);
        free(worker->result.derived_key);
        worker->result.derived_key = nullptr;
    }
    kdf_context_destroy(&worker->ctx);
    worker->active = false;
}

int derive_session_key(void *session_ptr, uint8_t *output, size_t out_len) {
    if (!session_ptr || !output) return -1;

    RotationSession *session = (RotationSession *)session_ptr;

    if (session->pending_key_ref) {
        memcpy(output, session->pending_key_ref->material, 
               out_len < CIPHER_KEY_SIZE ? out_len : CIPHER_KEY_SIZE);
        return 0;
    }

    memcpy(output, session->current_key.material,
           out_len < CIPHER_KEY_SIZE ? out_len : CIPHER_KEY_SIZE);
    return 0;
}

static int kdf_pbkdf2_derive(const uint8_t *password, size_t pw_len,
                             const uint8_t *salt, size_t salt_len,
                             uint32_t iterations, uint8_t *output, size_t out_len) {
    if (!password || !salt || !output) return -1;

    uint8_t block[KDF_OUTPUT_SIZE];
    uint8_t prev[KDF_OUTPUT_SIZE];
    size_t offset = 0;
    uint32_t block_num = 1;

    while (offset < out_len) {
        uint8_t seed[KDF_SALT_SIZE + 4 + 256];
        size_t seed_len = 0;

        memcpy(seed, salt, salt_len);
        seed_len += salt_len;

        seed[seed_len++] = (uint8_t)(block_num >> 24);
        seed[seed_len++] = (uint8_t)(block_num >> 16);
        seed[seed_len++] = (uint8_t)(block_num >> 8);
        seed[seed_len++] = (uint8_t)(block_num);

        size_t pw_copy = pw_len < 256 ? pw_len : 256;
        memcpy(seed + seed_len, password, pw_copy);
        seed_len += pw_copy;

        kdf_hash_block(seed, seed_len, prev, KDF_OUTPUT_SIZE);
        memcpy(block, prev, KDF_OUTPUT_SIZE);

        for (uint32_t iter = 1; iter < iterations; iter++) {
            kdf_hash_block(prev, KDF_OUTPUT_SIZE, prev, KDF_OUTPUT_SIZE);
            for (size_t j = 0; j < KDF_OUTPUT_SIZE; j++) {
                block[j] ^= prev[j];
            }
        }

        size_t chunk = out_len - offset;
        if (chunk > KDF_OUTPUT_SIZE) chunk = KDF_OUTPUT_SIZE;
        memcpy(output + offset, block, chunk);
        offset += chunk;
        block_num++;
    }

    secure_memzero(block, sizeof(block));
    secure_memzero(prev, sizeof(prev));
    return 0;
}

static int kdf_scrypt_romix(uint8_t *block, size_t block_len, uint32_t cost) {
    if (!block || block_len == 0 || cost == 0) return -1;

    size_t n = (size_t)1 << cost;
    if (n > 65536) n = 65536;

    uint8_t **v = (uint8_t **)malloc(n * sizeof(uint8_t *));
    if (!v) return -1;

    for (size_t i = 0; i < n; i++) {
        v[i] = (uint8_t *)malloc(block_len);
        if (!v[i]) {
            for (size_t j = 0; j < i; j++) free(v[j]);
            free(v);
            return -1;
        }
    }

    memcpy(v[0], block, block_len);
    for (size_t i = 1; i < n; i++) {
        memcpy(v[i], v[i - 1], block_len);
        for (size_t j = 0; j < block_len; j++) {
            v[i][j] ^= block[j % block_len];
            v[i][j] = (uint8_t)((v[i][j] << 1) | (v[i][j] >> 7));
        }
    }

    for (size_t i = 0; i < n; i++) {
        uint32_t idx = 0;
        memcpy(&idx, v[i], sizeof(uint32_t));
        idx %= (uint32_t)n;

        for (size_t j = 0; j < block_len; j++) {
            block[j] ^= v[idx][j];
        }
    }

    for (size_t i = 0; i < n; i++) {
        secure_memzero(v[i], block_len);
        free(v[i]);
    }
    free(v);
    return 0;
}

static void kdf_stretch_key(const uint8_t *key, size_t key_len,
                            uint8_t *output, size_t out_len) {
    if (!key || !output) return;

    size_t pos = 0;
    uint32_t counter = 0;

    while (pos < out_len) {
        uint8_t block_input[KDF_BLOCK_SIZE];
        memset(block_input, 0, KDF_BLOCK_SIZE);
        memcpy(block_input, key, key_len < KDF_BLOCK_SIZE ? key_len : KDF_BLOCK_SIZE);

        block_input[KDF_BLOCK_SIZE - 4] = (uint8_t)(counter >> 24);
        block_input[KDF_BLOCK_SIZE - 3] = (uint8_t)(counter >> 16);
        block_input[KDF_BLOCK_SIZE - 2] = (uint8_t)(counter >> 8);
        block_input[KDF_BLOCK_SIZE - 1] = (uint8_t)(counter);

        uint8_t block_output[KDF_BLOCK_SIZE];
        kdf_hash_block(block_input, KDF_BLOCK_SIZE, block_output, KDF_BLOCK_SIZE);

        size_t chunk = out_len - pos;
        if (chunk > KDF_BLOCK_SIZE) chunk = KDF_BLOCK_SIZE;
        memcpy(output + pos, block_output, chunk);
        pos += chunk;
        counter++;
    }
}
