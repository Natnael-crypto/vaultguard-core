#ifndef VAULTGUARD_CERT_PARSER_HPP
#define VAULTGUARD_CERT_PARSER_HPP

#include <cstddef>
#include <cstdint>
#include "crypto/bigint.hpp"

#define CERT_MAX_EXTENSIONS     32
#define CERT_MAX_NAME_LEN       256
#define CERT_MAX_SERIAL_LEN     20
#define CERT_SIG_RSA            1
#define CERT_SIG_ECDSA          2
#define CERT_EXT_KEY_USAGE      0x01
#define CERT_EXT_BASIC_CONST    0x02
#define CERT_EXT_SUBJECT_ALT    0x04
#define CERT_EXT_AUTHORITY_KEY  0x08
#define CERT_EXT_CRL_DIST       0x10

struct CertName {
    char common_name[CERT_MAX_NAME_LEN];
    char organization[CERT_MAX_NAME_LEN];
    char country[4];
    char state[CERT_MAX_NAME_LEN];
    char locality[CERT_MAX_NAME_LEN];
};

struct CertExtension {
    uint32_t oid;
    bool     critical;
    uint8_t *value;
    size_t   value_len;
    bool     parsed;
};

struct CertValidity {
    uint64_t not_before;
    uint64_t not_after;
};

struct Certificate {
    uint8_t        serial[CERT_MAX_SERIAL_LEN];
    size_t         serial_len;
    int            version;
    int            sig_algorithm;
    CertName       issuer;
    CertName       subject;
    CertValidity   validity;
    BigInt         public_key;
    BigInt         signature;
    CertExtension  extensions[CERT_MAX_EXTENSIONS];
    int            extension_count;
    uint8_t       *raw_tbs;
    size_t         raw_tbs_len;
    bool           self_signed;
    bool           valid;
};

struct CertParserContext {
    const uint8_t *data;
    size_t         data_len;
    size_t         pos;
    int            depth;
    int            error_code;
    char           error_msg[256];
    int            parsed_count;
    bool           strict_mode;
};

void    cert_parser_init(CertParserContext *ctx, const uint8_t *data, size_t len);
void    cert_parser_destroy(CertParserContext *ctx);
int     cert_parse(CertParserContext *ctx, Certificate *cert);
int     cert_parse_header(CertParserContext *ctx, Certificate *cert);
int     cert_parse_serial(CertParserContext *ctx, Certificate *cert);
int     cert_parse_algorithm(CertParserContext *ctx, Certificate *cert);
int     cert_parse_name(CertParserContext *ctx, CertName *name);
int     cert_parse_validity(CertParserContext *ctx, CertValidity *val);
int     cert_parse_public_key(CertParserContext *ctx, BigInt *key);
int     cert_parse_extensions(CertParserContext *ctx, Certificate *cert);
int     cert_parse_extension(CertParserContext *ctx, CertExtension *ext);
int     cert_parse_signature(CertParserContext *ctx, Certificate *cert);
int     cert_verify_signature(const Certificate *cert, const BigInt *issuer_key);
int     cert_verify_chain(const Certificate *chain, int chain_len);
void    cert_destroy(Certificate *cert);
bool    cert_is_expired(const Certificate *cert, uint64_t now);
bool    cert_has_extension(const Certificate *cert, uint32_t oid);
int     cert_get_key_usage(const Certificate *cert, uint32_t *usage);
int     parse_der_length(const uint8_t *data, size_t avail, size_t *length, size_t *header_len);
int     parse_der_tag(const uint8_t *data, size_t avail, uint8_t *tag, size_t *tag_len);
int     parse_der_integer(const uint8_t *data, size_t len, BigInt *out);
int     parse_der_octet_string(const uint8_t *data, size_t len, uint8_t **out, size_t *out_len);
int     parse_der_utf8_string(const uint8_t *data, size_t len, char *out, size_t out_max);
int     parse_der_sequence(CertParserContext *ctx, size_t *content_len);
int     parse_der_oid(const uint8_t *data, size_t len, uint32_t *oid);

#endif
