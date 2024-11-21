#include <cassert>
#include <cstdio>
#include <cstring>
#include "crypto/bigint.hpp"
#include "crypto/cipher_stream.hpp"
#include "crypto/cert_parser.hpp"

void test_bigint() {
    BigInt a, b, c;
    bigint_init(&a);
    bigint_init(&b);
    bigint_init(&c);

    bigint_from_uint64(&a, 100);
    bigint_from_uint64(&b, 200);
    bigint_add(&c, &a, &b);
    assert(bigint_to_uint64(&c) == 300);

    bigint_multiply(&c, &a, &b);
    assert(bigint_to_uint64(&c) == 20000);
}

void test_cipher() {
    uint8_t key[CIPHER_KEY_SIZE] = {0};
    uint8_t nonce[CIPHER_NONCE_SIZE] = {0};
    key[0] = 1; nonce[0] = 2;

    CipherState cs;
    cipher_init(&cs, key, nonce);

    uint8_t pt[] = "test data";
    uint8_t ct[64] = {0};
    uint8_t dec[64] = {0};
    
    cipher_encrypt_block(&cs, pt, ct, sizeof(pt));
    
    cipher_init(&cs, key, nonce);
    cipher_decrypt_block(&cs, ct, dec, sizeof(pt));

    assert(memcmp(pt, dec, sizeof(pt)) == 0);
    cipher_destroy(&cs);
}

int main() {
    test_bigint();
    test_cipher();
    printf("test_crypto passed\n");
    return 0;
}
