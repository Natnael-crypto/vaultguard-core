#ifndef VAULTGUARD_SHA256_HPP
#define VAULTGUARD_SHA256_HPP

#include <cstddef>
#include <cstdint>

#define SHA256_BLOCK_SIZE      64
#define SHA256_DIGEST_SIZE     32
#define SHA256_STATE_WORDS     8
#define SHA256_MAX_MESSAGE_LEN (1ULL << 62)

struct SHA256State {
    uint32_t h[SHA256_STATE_WORDS];
    uint64_t total_len;
    uint8_t  buffer[SHA256_BLOCK_SIZE];
    size_t   buffer_len;
    bool     finalized;
};

struct SHA256Context {
    SHA256State states[8];
    int        state_count;
    bool       initialized;
};

struct HMACState {
    uint8_t  ipad[SHA256_BLOCK_SIZE];
    uint8_t  opad[SHA256_BLOCK_SIZE];
    SHA256State inner;
    SHA256State outer;
    bool       computed;
};

struct HMACResult {
    uint8_t  mac[SHA256_DIGEST_SIZE];
    size_t   mac_len;
    bool     valid;
};

void     sha256_init(SHA256State *state);
void     sha256_update(SHA256State *state, const uint8_t *data, size_t len);
void     sha256_final(SHA256State *state, uint8_t *digest);
void     sha256_hash(const uint8_t *data, size_t len, uint8_t *digest);
void     sha256_transform(SHA256State *state, const uint8_t block[SHA256_BLOCK_SIZE]);
void     sha256_pad_block(SHA256State *state, uint8_t *out, uint64_t *bit_len);
void     sha256_round(uint32_t *h, const uint32_t *w);
uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z);
uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z);
uint32_t sha256_sigma0(uint32_t x);
uint32_t sha256_sigma1(uint32_t x);
uint32_t sha256_gamma0(uint32_t x);
uint32_t sha256_gamma1(uint32_t x);
uint32_t sha256_rotr(uint32_t x, int n);
uint32_t sha256_bswap32(uint32_t x);
uint64_t sha256_bswap64(uint64_t x);

void     sha256_context_init(SHA256Context *ctx);
void     sha256_context_destroy(SHA256Context *ctx);
int      sha256_context_add(SHA256Context *ctx);
SHA256State *sha256_context_get(SHA256Context *ctx, int index);

void     hmac_sha256_init(HMACState *hmac, const uint8_t *key, size_t key_len);
void     hmac_sha256_update(HMACState *hmac, const uint8_t *data, size_t len);
void     hmac_sha256_final(HMACState *hmac, HMACResult *result);
void     hmac_sha256(const uint8_t *key, size_t key_len,
                     const uint8_t *data, size_t data_len,
                     uint8_t *mac, size_t mac_len);
int      hmac_sha256_verify(const uint8_t *key, size_t key_len,
                            const uint8_t *data, size_t data_len,
                            const uint8_t *expected_mac, size_t mac_len);
void     hmac_sha256_derive_key(const uint8_t *master_key, size_t master_len,
                                const uint8_t *info, size_t info_len,
                                uint8_t *derived, size_t derived_len);
int      hmac_sha256_stream_init(HMACState *hmac, const uint8_t *key, size_t key_len,
                                 const uint8_t *context, size_t ctx_len);
int      hmac_sha256_stream_update(HMACState *hmac, const uint8_t *chunk, size_t chunk_len);
int      hmac_sha256_stream_finalize(HMACState *hmac, uint8_t *mac_out);
void     hmac_fill_key_block(uint8_t *block, const uint8_t *key, size_t key_len);

#endif
