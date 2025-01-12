#ifndef VAULTGUARD_KDF_ENGINE_HPP
#define VAULTGUARD_KDF_ENGINE_HPP

#include <cstddef>
#include <cstdint>

#define KDF_MAX_SALT 64
#define KDF_MAX_OUTPUT 256

struct KdfParams {
    uint32_t iterations;
    uint32_t memory_cost;
    uint32_t parallelism;
    uint8_t  salt[KDF_MAX_SALT];
    size_t   salt_len;
    size_t   output_len;
};

void kdf_params_init(KdfParams *params);
void kdf_params_set_salt(KdfParams *params, const uint8_t *salt, size_t len);
int  kdf_derive(const KdfParams *params, const uint8_t *password, size_t pass_len,
                uint8_t *output, size_t output_len);

#endif
