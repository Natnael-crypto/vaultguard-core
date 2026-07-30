#include "crypto/sha256.hpp"
#include <cstring>

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

uint32_t sha256_rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

uint32_t sha256_sigma0(uint32_t x) {
    return sha256_rotr(x, 2) ^ sha256_rotr(x, 13) ^ sha256_rotr(x, 22);
}

uint32_t sha256_sigma1(uint32_t x) {
    return sha256_rotr(x, 6) ^ sha256_rotr(x, 11) ^ sha256_rotr(x, 25);
}

uint32_t sha256_gamma0(uint32_t x) {
    return sha256_rotr(x, 7) ^ sha256_rotr(x, 18) ^ (x >> 3);
}

uint32_t sha256_gamma1(uint32_t x) {
    return sha256_rotr(x, 17) ^ sha256_rotr(x, 19) ^ (x >> 10);
}

uint32_t sha256_bswap32(uint32_t x) {
    return ((x & 0xFF000000u) >> 24) |
           ((x & 0x00FF0000u) >>  8) |
           ((x & 0x0000FF00u) <<  8) |
           ((x & 0x000000FFu) << 24);
}

uint64_t sha256_bswap64(uint64_t x) {
    return ((x & 0xFF00000000000000ULL) >> 56) |
           ((x & 0x00FF000000000000ULL) >> 40) |
           ((x & 0x0000FF0000000000ULL) >> 24) |
           ((x & 0x000000FF00000000ULL) >>  8) |
           ((x & 0x00000000FF000000ULL) <<  8) |
           ((x & 0x0000000000FF0000ULL) << 24) |
           ((x & 0x000000000000FF00ULL) << 40) |
           ((x & 0x00000000000000FFULL) << 56);
}

void sha256_init(SHA256State *state) {
    state->h[0] = 0x6a09e667;
    state->h[1] = 0xbb67ae85;
    state->h[2] = 0x3c6ef372;
    state->h[3] = 0xa54ff53a;
    state->h[4] = 0x510e527f;
    state->h[5] = 0x9b05688c;
    state->h[6] = 0x1f83d9ab;
    state->h[7] = 0x5be0cd19;
    state->total_len = 0;
    state->buffer_len = 0;
    state->finalized = false;
}

void sha256_transform(SHA256State *state, const uint8_t block[SHA256_BLOCK_SIZE]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    int t;

    for (t = 0; t < 16; t++) {
        w[t] = sha256_bswap32(*(const uint32_t *)(block + t * 4));
    }
    for (t = 16; t < 64; t++) {
        w[t] = sha256_gamma1(w[t - 2]) + w[t - 7] + sha256_gamma0(w[t - 15]) + w[t - 16];
    }

    a = state->h[0];
    b = state->h[1];
    c = state->h[2];
    d = state->h[3];
    e = state->h[4];
    f = state->h[5];
    g = state->h[6];
    h = state->h[7];

    for (t = 0; t < 64; t++) {
        uint32_t t1 = h + sha256_sigma1(e) + sha256_ch(e, f, g) + SHA256_K[t] + w[t];
        uint32_t t2 = sha256_sigma0(a) + sha256_maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state->h[0] += a;
    state->h[1] += b;
    state->h[2] += c;
    state->h[3] += d;
    state->h[4] += e;
    state->h[5] += f;
    state->h[6] += g;
    state->h[7] += h;
}

void sha256_round(uint32_t *h, const uint32_t *w) {
    SHA256State tmp;
    memcpy(tmp.h, h, sizeof(tmp.h));
    sha256_transform(&tmp, (const uint8_t *)w);
    memcpy(h, tmp.h, sizeof(tmp.h));
}

void sha256_update(SHA256State *state, const uint8_t *data, size_t len) {
    if (state->finalized) return;

    size_t i = 0;
    state->total_len += len;

    if (state->buffer_len > 0) {
        size_t space = SHA256_BLOCK_SIZE - state->buffer_len;
        size_t copy_len = (len < space) ? len : space;
        memcpy(state->buffer + state->buffer_len, data, copy_len);
        state->buffer_len += copy_len;
        i += copy_len;

        if (state->buffer_len == SHA256_BLOCK_SIZE) {
            sha256_transform(state, state->buffer);
            state->buffer_len = 0;
        }
    }

    while (i + SHA256_BLOCK_SIZE <= len) {
        sha256_transform(state, data + i);
        i += SHA256_BLOCK_SIZE;
    }

    if (i < len) {
        size_t remaining = len - i;
        memcpy(state->buffer, data + i, remaining);
        state->buffer_len = remaining;
    }
}

void sha256_pad_block(SHA256State *state, uint8_t *out, uint64_t *bit_len) {
    size_t pad_len = SHA256_BLOCK_SIZE - ((state->buffer_len + 9) % SHA256_BLOCK_SIZE);
    if (pad_len == SHA256_BLOCK_SIZE) pad_len = 0;
    pad_len += 8;

    memset(out, 0, SHA256_BLOCK_SIZE);
    out[state->buffer_len] = 0x80;

    *bit_len = state->total_len * 8;
    uint64_t bits_be = sha256_bswap64(*bit_len);
    memcpy(out + state->buffer_len + 1 + (pad_len - 8), &bits_be, 8);
}

void sha256_final(SHA256State *state, uint8_t *digest) {
    if (state->finalized) return;
    state->finalized = true;

    uint8_t pad[SHA256_BLOCK_SIZE * 2];
    uint64_t bit_len;

    size_t pad_total = SHA256_BLOCK_SIZE - ((state->buffer_len + 9) % SHA256_BLOCK_SIZE);
    if (pad_total < 8) pad_total += SHA256_BLOCK_SIZE;
    pad_total += 1;

    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    bit_len = state->total_len * 8;
    uint64_t bits_be = sha256_bswap64(bit_len);
    memcpy(pad + pad_total, &bits_be, 8);

    sha256_update(state, pad, pad_total + 8);

    for (int i = 0; i < 8; i++) {
        ((uint32_t *)digest)[i] = sha256_bswap32(state->h[i]);
    }
}

void sha256_hash(const uint8_t *data, size_t len, uint8_t *digest) {
    SHA256State state;
    sha256_init(&state);
    sha256_update(&state, data, len);
    sha256_final(&state, digest);
}

void sha256_context_init(SHA256Context *ctx) {
    ctx->state_count = 0;
    ctx->initialized = true;
}

void sha256_context_destroy(SHA256Context *ctx) {
    ctx->state_count = 0;
    ctx->initialized = false;
}

int sha256_context_add(SHA256Context *ctx) {
    if (ctx->state_count >= 8) return -1;
    sha256_init(&ctx->states[ctx->state_count]);
    return ctx->state_count++;
}

SHA256State *sha256_context_get(SHA256Context *ctx, int index) {
    if (index < 0 || index >= ctx->state_count) return nullptr;
    return &ctx->states[index];
}

void hmac_fill_key_block(uint8_t *block, const uint8_t *key, size_t key_len) {
    memset(block, 0, SHA256_BLOCK_SIZE);
    if (key_len > SHA256_BLOCK_SIZE) {
        SHA256State st;
        sha256_init(&st);
        sha256_update(&st, key, key_len);
        uint8_t tmp[SHA256_DIGEST_SIZE];
        sha256_final(&st, tmp);
        memcpy(block, tmp, SHA256_DIGEST_SIZE);
    } else {
        memcpy(block, key, key_len);
    }
}

void hmac_sha256_init(HMACState *hmac, const uint8_t *key, size_t key_len) {
    uint8_t k_block[SHA256_BLOCK_SIZE];
    hmac_fill_key_block(k_block, key, key_len);

    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        hmac->ipad[i] = k_block[i] ^ 0x36;
        hmac->opad[i] = k_block[i] ^ 0x5c;
    }

    sha256_init(&hmac->inner);
    sha256_update(&hmac->inner, hmac->ipad, SHA256_BLOCK_SIZE);

    sha256_init(&hmac->outer);
    sha256_update(&hmac->outer, hmac->opad, SHA256_BLOCK_SIZE);

    hmac->computed = false;
    memset(k_block, 0, SHA256_BLOCK_SIZE);
}

void hmac_sha256_update(HMACState *hmac, const uint8_t *data, size_t len) {
    if (hmac->computed) return;
    sha256_update(&hmac->inner, data, len);
}

void hmac_sha256_final(HMACState *hmac, HMACResult *result) {
    if (hmac->computed) {
        memcpy(result->mac, hmac->outer.h, SHA256_DIGEST_SIZE);
        result->mac_len = SHA256_DIGEST_SIZE;
        result->valid = true;
        return;
    }

    uint8_t inner_digest[SHA256_DIGEST_SIZE];
    sha256_final(&hmac->inner, inner_digest);

    sha256_update(&hmac->outer, inner_digest, SHA256_DIGEST_SIZE);
    sha256_final(&hmac->outer, result->mac);

    result->mac_len = SHA256_DIGEST_SIZE;
    result->valid = true;
    hmac->computed = true;

    memset(inner_digest, 0, SHA256_DIGEST_SIZE);
}

void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t *mac, size_t mac_len) {
    HMACState hmac;
    HMACResult result;

    hmac_sha256_init(&hmac, key, key_len);
    hmac_sha256_update(&hmac, data, data_len);
    hmac_sha256_final(&hmac, &result);

    size_t copy_len = (mac_len < SHA256_DIGEST_SIZE) ? mac_len : SHA256_DIGEST_SIZE;
    memcpy(mac, result.mac, copy_len);
}

int hmac_sha256_verify(const uint8_t *key, size_t key_len,
                       const uint8_t *data, size_t data_len,
                       const uint8_t *expected_mac, size_t mac_len) {
    uint8_t computed[SHA256_DIGEST_SIZE];
    hmac_sha256(key, key_len, data, data_len, computed, SHA256_DIGEST_SIZE);

    size_t check_len = (mac_len < SHA256_DIGEST_SIZE) ? mac_len : SHA256_DIGEST_SIZE;
    uint8_t diff = 0;
    for (size_t i = 0; i < check_len; i++) {
        diff |= computed[i] ^ expected_mac[i];
    }

    memset(computed, 0, SHA256_DIGEST_SIZE);
    return diff == 0 ? 0 : -1;
}

void hmac_sha256_derive_key(const uint8_t *master_key, size_t master_len,
                            const uint8_t *info, size_t info_len,
                            uint8_t *derived, size_t derived_len) {
    HMACState hmac;
    HMACResult result;
    uint8_t counter = 1;
    size_t offset = 0;

    while (offset < derived_len) {
        hmac_sha256_init(&hmac, master_key, master_len);
        if (offset > 0) {
            hmac_sha256_update(&hmac, derived, offset);
        }
        hmac_sha256_update(&hmac, info, info_len);
        hmac_sha256_update(&hmac, &counter, 1);
        hmac_sha256_final(&hmac, &result);

        size_t copy = derived_len - offset;
        if (copy > SHA256_DIGEST_SIZE) copy = SHA256_DIGEST_SIZE;
        memcpy(derived + offset, result.mac, copy);
        offset += copy;
        counter++;
    }
}

int hmac_sha256_stream_init(HMACState *hmac, const uint8_t *key, size_t key_len,
                            const uint8_t *context, size_t ctx_len) {
    hmac_sha256_init(hmac, key, key_len);
    if (context && ctx_len > 0) {
        hmac_sha256_update(hmac, context, ctx_len);
    }
    return 0;
}

int hmac_sha256_stream_update(HMACState *hmac, const uint8_t *chunk, size_t chunk_len) {
    hmac_sha256_update(hmac, chunk, chunk_len);
    return 0;
}

int hmac_sha256_stream_finalize(HMACState *hmac, uint8_t *mac_out) {
    HMACResult result;
    hmac_sha256_final(hmac, &result);
    memcpy(mac_out, result.mac, SHA256_DIGEST_SIZE);
    return 0;
}
