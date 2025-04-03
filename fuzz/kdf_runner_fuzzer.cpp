#include <cstdint>
#include <cstddef>
#include "crypto/kdf_engine.hpp"
#include "crypto/cipher_stream.hpp"
#include "crypto/cert_parser.hpp"
#include "session/key_rotation.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    KdfParams params;
    kdf_params_init(&params);
    params.iterations = (data[0] % 100) + 1;
    kdf_params_set_salt(&params, data, size > 64 ? 64 : size);

    uint8_t output[32];
    kdf_derive(&params, data, size, output, sizeof(output));

    CipherState cs;
    cipher_init(&cs, data, data + 32 < data + size ? data + 32 : data);
    uint8_t ct[64];
    cipher_encrypt_block(&cs, data, ct, size > 64 ? 64 : size);
    cipher_destroy(&cs);

    return 0;
}
