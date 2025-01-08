#ifndef VAULTGUARD_CIPHER_STREAM_HPP
#define VAULTGUARD_CIPHER_STREAM_HPP

#include <cstddef>
#include <cstdint>

#define CIPHER_KEY_SIZE    32
#define CIPHER_NONCE_SIZE  12
#define CIPHER_BLOCK_SIZE  16

struct CipherKey {
    uint8_t  material[CIPHER_KEY_SIZE];
    uint32_t key_id;
    bool     derived;
    uint32_t usage_count;
};

struct CipherState {
    uint8_t key[CIPHER_KEY_SIZE];
    uint8_t nonce[CIPHER_NONCE_SIZE];
    uint8_t counter;
};

void cipher_init(CipherState *cs, const uint8_t *key, const uint8_t *nonce);
void cipher_destroy(CipherState *cs);
void cipher_encrypt_block(CipherState *cs, const uint8_t *in, uint8_t *out, size_t len);
void cipher_decrypt_block(CipherState *cs, const uint8_t *in, uint8_t *out, size_t len);
int  payload_process_header(const uint8_t *data, size_t total_len,
                            uint8_t *mac_out, size_t *mac_len,
                            uint8_t *padding_out, size_t *padding_len,
                            uint8_t *payload_out, size_t *payload_len);

#endif
