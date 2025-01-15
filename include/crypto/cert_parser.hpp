#ifndef VAULTGUARD_CERT_PARSER_HPP
#define VAULTGUARD_CERT_PARSER_HPP

#include <cstddef>
#include <cstdint>

struct CertInfo {
    char     subject[256];
    char     issuer[256];
    uint8_t  serial[32];
    size_t   serial_len;
};

int cert_parse(const uint8_t *data, size_t len, CertInfo *info);
int cert_verify_signature(const uint8_t *cert_data, size_t cert_len,
                          const uint8_t *ca_data, size_t ca_len);

#endif
