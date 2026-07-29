#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "allocator/secure_arena.hpp"
#include "crypto/cipher_stream.hpp"
#include "crypto/kdf_engine.hpp"
#include "crypto/cert_parser.hpp"
#include "crypto/bigint.hpp"
#include "session/key_rotation.hpp"
#include "session/session_manager.hpp"

#ifdef __cplusplus
}
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return 0;

    uint8_t mode = data[0];
    uint8_t sub_mode = data[1];
    const uint8_t *payload = data + 4;
    size_t payload_len = size - 4;

    if ((mode % 4) == 0) {
        if (payload_len >= 12) {
            EncryptedPayload ep;
            memset(&ep, 0, sizeof(ep));
            payload_process_header(payload, payload_len, &ep);
            payload_free(&ep);
        }
    }

    if ((mode % 4) == 1) {
        if (payload_len > 16) {
            CipherKey key;
            memset(&key, 0, sizeof(key));
            memcpy(key.material, payload, payload_len > CIPHER_KEY_SIZE ? CIPHER_KEY_SIZE : payload_len);
            key.key_id = 1;

            const uint8_t *pt = payload + CIPHER_KEY_SIZE;
            size_t pt_len = payload_len > CIPHER_KEY_SIZE ? payload_len - CIPHER_KEY_SIZE : 0;
            if (pt_len > 0 && pt_len < 4096) {
                EncryptedPayload ep;
                if (payload_encrypt(&key, pt, pt_len, &ep) == 0) {
                    uint8_t *dec = (uint8_t *)malloc(ep.data_len + 64);
                    if (dec) {
                        size_t dec_len;
                        payload_decrypt(&key, &ep, dec, &dec_len);
                        free(dec);
                    }
                    payload_free(&ep);
                }
            }
        }
    }

    if ((mode % 4) == 2) {
        RotationSession rs;
        rotation_session_init(&rs);
        rotation_generate_key_material(&rs.current_key, 0);

        rotation_begin(&rs);
        rotation_generate(&rs);

        uint32_t acl_flags = 0;
        if (payload_len > 0) {
            acl_flags = payload[0];
        }
        rotation_commit(&rs, acl_flags);

        if (rs.stage == ROTATION_STAGE_COMMIT) {
            rotation_finalize(&rs);
        }

        if ((sub_mode % 2) == 0 && rs.pending_key_ref) {
            uint8_t key_out[32];
            derive_session_key(&rs, key_out, sizeof(key_out));
        }

        rotation_session_destroy(&rs);
    }

    if ((mode % 4) == 3) {
        if (payload_len > 8) {
            CertParserContext ctx;
            cert_parser_init(&ctx, payload, payload_len);
            Certificate cert;
            cert_parse(&ctx, &cert);
            cert_destroy(&cert);
            cert_parser_destroy(&ctx);
        }
    }

    return 0;
}
