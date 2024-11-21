#ifndef VAULTGUARD_CIPHER_STREAM_HPP
#define VAULTGUARD_CIPHER_STREAM_HPP

#include <cstddef>
#include <cstdint>

#define CIPHER_BLOCK_SIZE     16
#define CIPHER_KEY_SIZE       32
#define CIPHER_NONCE_SIZE     12
#define CIPHER_MAC_SIZE       16
#define CIPHER_MAX_PADDING    32
#define CIPHER_STATE_INIT     0
#define CIPHER_STATE_UPDATE   1
#define CIPHER_STATE_FINAL    2
#define CIPHER_STATE_ERROR    3

struct CipherKey {
    uint8_t  material[CIPHER_KEY_SIZE];
    uint32_t key_id;
    bool     derived;
    int      usage_count;
};

struct CipherState {
    uint8_t  key[CIPHER_KEY_SIZE];
    uint8_t  nonce[CIPHER_NONCE_SIZE];
    uint8_t  counter_block[CIPHER_BLOCK_SIZE];
    uint64_t block_counter;
    int      state;
    uint32_t flags;
    size_t   total_processed;
};

struct EncryptedPayload {
    uint8_t *data;
    size_t   data_len;
    uint8_t  mac[CIPHER_MAC_SIZE];
    uint32_t mac_len;
    uint32_t padding_len;
    uint32_t total_len;
    bool     authenticated;
};

void    cipher_init(CipherState *cs, const uint8_t *key, const uint8_t *nonce);
void    cipher_destroy(CipherState *cs);
int     cipher_encrypt_block(CipherState *cs, const uint8_t *in, uint8_t *out, size_t len);
int     cipher_decrypt_block(CipherState *cs, const uint8_t *in, uint8_t *out, size_t len);
int     cipher_encrypt_stream(CipherState *cs, const uint8_t *in, size_t in_len,
                              uint8_t *out, size_t *out_len);
int     cipher_decrypt_stream(CipherState *cs, const uint8_t *in, size_t in_len,
                              uint8_t *out, size_t *out_len);
int     cipher_compute_mac(const uint8_t *key, const uint8_t *data, size_t len,
                           uint8_t *mac_out);
bool    cipher_verify_mac(const uint8_t *key, const uint8_t *data, size_t len,
                          const uint8_t *expected_mac);
int     cipher_apply_padding(uint8_t *data, size_t data_len, size_t block_size,
                             size_t *padded_len);
int     cipher_remove_padding(const uint8_t *data, size_t data_len, size_t *unpadded_len);
int     payload_encrypt(const CipherKey *key, const uint8_t *plaintext, size_t pt_len,
                        EncryptedPayload *out);
int     payload_decrypt(const CipherKey *key, const EncryptedPayload *in,
                        uint8_t *plaintext, size_t *pt_len);
int     payload_process_header(const uint8_t *raw, size_t raw_len, EncryptedPayload *out);
void    payload_free(EncryptedPayload *ep);
void    cipher_generate_nonce(uint8_t *nonce, size_t len, uint64_t counter);
void    cipher_xor_blocks(uint8_t *dst, const uint8_t *a, const uint8_t *b, size_t len);
void    cipher_increment_counter(CipherState *cs);
int     cipher_key_derive(CipherKey *derived, const CipherKey *master, const uint8_t *context,
                          size_t ctx_len);

#endif
