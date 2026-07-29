#include "crypto/cert_parser.hpp"
#include "allocator/secure_arena.hpp"
#include "tlv/tlv_decoder.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

void cert_parser_init(CertParserContext *ctx, const uint8_t *data, size_t len) {
    memset(ctx, 0, sizeof(CertParserContext));
    ctx->data = data;
    ctx->data_len = len;
    ctx->pos = 0;
    ctx->depth = 0;
    ctx->strict_mode = true;
}

void cert_parser_destroy(CertParserContext *ctx) {
    if (!ctx) return;
    ctx->data = nullptr;
    ctx->data_len = 0;
}

int parse_der_tag(const uint8_t *data, size_t avail, uint8_t *tag, size_t *tag_len) {
    if (!data || avail == 0 || !tag) return -1;
    *tag = data[0];
    if (tag_len) *tag_len = 1;
    return 0;
}

int parse_der_length(const uint8_t *data, size_t avail, size_t *length, size_t *header_len) {
    if (!data || avail == 0 || !length || !header_len) return -1;

    if (data[0] < 0x80) {
        *length = data[0];
        *header_len = 1;
        return 0;
    }

    if (data[0] == 0x80) {
        *length = 0;
        *header_len = 1;
        return 0;
    }

    int num_bytes = data[0] & 0x7F;
    if (num_bytes > 4 || (size_t)(num_bytes + 1) > avail) return -1;

    size_t val = 0;
    for (int i = 0; i < num_bytes; i++) {
        val = (val << 8) | data[1 + i];
    }
    *length = val;
    *header_len = 1 + num_bytes;
    return 0;
}

int parse_der_integer(const uint8_t *data, size_t len, BigInt *out) {
    if (!data || !out || len == 0) return -1;
    return bigint_set_bytes(out, data, len);
}

int parse_der_octet_string(const uint8_t *data, size_t len, uint8_t **out, size_t *out_len) {
    if (!data || !out || !out_len) return -1;
    *out = (uint8_t *)malloc(len);
    if (!*out) return -1;
    memcpy(*out, data, len);
    *out_len = len;
    return 0;
}

int parse_der_utf8_string(const uint8_t *data, size_t len, char *out, size_t out_max) {
    if (!data || !out) return -1;
    size_t copy_len = len < out_max - 1 ? len : out_max - 1;
    memcpy(out, data, copy_len);
    out[copy_len] = '\0';
    return 0;
}

int parse_der_oid(const uint8_t *data, size_t len, uint32_t *oid) {
    if (!data || !oid || len == 0) return -1;
    uint32_t val = 0;
    for (size_t i = 0; i < len; i++) {
        val = (val << 7) | (data[i] & 0x7F);
        if (!(data[i] & 0x80)) break;
    }
    *oid = val;
    return 0;
}

int parse_der_sequence(CertParserContext *ctx, size_t *content_len) {
    if (!ctx || !content_len) return -1;
    if (ctx->pos >= ctx->data_len) return -1;

    uint8_t tag;
    size_t tlen;
    if (parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen) != 0)
        return -1;

    if (tag != TLV_TAG_SEQUENCE && tag != TLV_TAG_SET &&
        (tag & 0xC0) != 0x80 && (tag & 0xC0) != 0xA0)
        return -1;

    ctx->pos += tlen;

    size_t length, hlen;
    if (parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &length, &hlen) != 0)
        return -1;

    ctx->pos += hlen;
    *content_len = length;
    return 0;
}

int cert_parse_header(CertParserContext *ctx, Certificate *cert) {
    if (!ctx || !cert) return -1;

    size_t seq_len;
    if (parse_der_sequence(ctx, &seq_len) != 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "invalid outer sequence");
        return -1;
    }

    size_t tbs_start = ctx->pos;

    size_t tbs_len;
    if (parse_der_sequence(ctx, &tbs_len) != 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "invalid TBS sequence");
        return -1;
    }

    cert->raw_tbs = (uint8_t *)malloc(tbs_len);
    if (cert->raw_tbs && tbs_len <= ctx->data_len - ctx->pos) {
        memcpy(cert->raw_tbs, ctx->data + ctx->pos, tbs_len);
        cert->raw_tbs_len = tbs_len;
    }

    if (ctx->pos < ctx->data_len && ctx->data[ctx->pos] == TLV_TAG_CONTEXT_0) {
        ctx->pos++;
        size_t vlen, vhlen;
        if (parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &vlen, &vhlen) == 0) {
            ctx->pos += vhlen;
            if (vlen > 0 && ctx->pos < ctx->data_len) {
                cert->version = ctx->data[ctx->pos + vlen - 1];
                ctx->pos += vlen;
            }
        }
    } else {
        cert->version = 0;
    }

    return 0;
}

int cert_parse_serial(CertParserContext *ctx, Certificate *cert) {
    if (!ctx || !cert) return -1;
    if (ctx->pos >= ctx->data_len) return -1;

    uint8_t tag;
    size_t tlen;
    parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
    if (tag != TLV_TAG_INTEGER) return -1;
    ctx->pos += tlen;

    size_t slen, shlen;
    if (parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &slen, &shlen) != 0)
        return -1;
    ctx->pos += shlen;

    if (slen > CERT_MAX_SERIAL_LEN) slen = CERT_MAX_SERIAL_LEN;
    if (ctx->pos + slen <= ctx->data_len) {
        memcpy(cert->serial, ctx->data + ctx->pos, slen);
        cert->serial_len = slen;
    }
    ctx->pos += slen;
    return 0;
}

int cert_parse_algorithm(CertParserContext *ctx, Certificate *cert) {
    if (!ctx || !cert) return -1;

    size_t alg_len;
    if (parse_der_sequence(ctx, &alg_len) != 0) return -1;

    if (ctx->pos + alg_len <= ctx->data_len) {
        uint8_t tag;
        size_t tlen;
        parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
        if (tag == TLV_TAG_OID) {
            ctx->pos += tlen;
            size_t olen, ohlen;
            parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &olen, &ohlen);
            ctx->pos += ohlen;
            if (olen > 0 && ctx->pos < ctx->data_len) {
                uint32_t oid_val;
                parse_der_oid(ctx->data + ctx->pos, olen, &oid_val);
                cert->sig_algorithm = (oid_val & 0x01) ? CERT_SIG_RSA : CERT_SIG_ECDSA;
                ctx->pos += olen;
            }
        } else {
            ctx->pos += alg_len;
        }
    } else {
        ctx->pos += alg_len;
    }

    return 0;
}

int cert_parse_name(CertParserContext *ctx, CertName *name) {
    if (!ctx || !name) return -1;

    size_t name_len;
    if (parse_der_sequence(ctx, &name_len) != 0) return -1;

    size_t name_end = ctx->pos + name_len;
    if (name_end > ctx->data_len) name_end = ctx->data_len;

    int field_idx = 0;
    while (ctx->pos < name_end && field_idx < 5) {
        size_t set_len;
        if (parse_der_sequence(ctx, &set_len) != 0) break;
        size_t set_end = ctx->pos + set_len;
        if (set_end > name_end) break;

        size_t attr_len;
        if (parse_der_sequence(ctx, &attr_len) != 0) { ctx->pos = set_end; continue; }

        uint8_t tag;
        size_t tlen;
        if (ctx->pos >= ctx->data_len) break;
        parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);

        if (tag == TLV_TAG_OID) {
            ctx->pos += tlen;
            size_t olen, ohlen;
            parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &olen, &ohlen);
            ctx->pos += ohlen;

            uint32_t oid_val = 0;
            if (olen > 0 && ctx->pos + olen <= ctx->data_len) {
                parse_der_oid(ctx->data + ctx->pos, olen, &oid_val);
            }
            ctx->pos += olen;

            if (ctx->pos < set_end) {
                parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
                ctx->pos += tlen;
                size_t vlen, vhlen;
                parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &vlen, &vhlen);
                ctx->pos += vhlen;

                char *dst = NULL;
                size_t dst_max = 0;
                switch (field_idx) {
                    case 0: dst = name->country; dst_max = sizeof(name->country); break;
                    case 1: dst = name->state; dst_max = sizeof(name->state); break;
                    case 2: dst = name->locality; dst_max = sizeof(name->locality); break;
                    case 3: dst = name->organization; dst_max = sizeof(name->organization); break;
                    case 4: dst = name->common_name; dst_max = sizeof(name->common_name); break;
                }
                if (dst && vlen > 0 && ctx->pos + vlen <= ctx->data_len) {
                    parse_der_utf8_string(ctx->data + ctx->pos, vlen, dst, dst_max);
                }
                ctx->pos += vlen;
            }
        }

        ctx->pos = set_end;
        field_idx++;
    }

    ctx->pos = name_end;
    return 0;
}

int cert_parse_validity(CertParserContext *ctx, CertValidity *val) {
    if (!ctx || !val) return -1;

    size_t vlen;
    if (parse_der_sequence(ctx, &vlen) != 0) return -1;

    size_t vend = ctx->pos + vlen;
    if (vend > ctx->data_len) vend = ctx->data_len;

    if (ctx->pos + 2 < vend) {
        uint8_t tag;
        size_t tlen;
        parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
        ctx->pos += tlen;
        size_t timelen, timehlen;
        parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &timelen, &timehlen);
        ctx->pos += timehlen;

        uint64_t timestamp = 0;
        for (size_t i = 0; i < timelen && i < 8 && ctx->pos + i < ctx->data_len; i++) {
            timestamp = (timestamp << 8) | ctx->data[ctx->pos + i];
        }
        val->not_before = timestamp;
        ctx->pos += timelen;
    }

    if (ctx->pos + 2 < vend) {
        uint8_t tag;
        size_t tlen;
        parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
        ctx->pos += tlen;
        size_t timelen, timehlen;
        parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &timelen, &timehlen);
        ctx->pos += timehlen;

        uint64_t timestamp = 0;
        for (size_t i = 0; i < timelen && i < 8 && ctx->pos + i < ctx->data_len; i++) {
            timestamp = (timestamp << 8) | ctx->data[ctx->pos + i];
        }
        val->not_after = timestamp;
        ctx->pos += timelen;
    }

    ctx->pos = vend;
    return 0;
}

int cert_parse_public_key(CertParserContext *ctx, BigInt *key) {
    if (!ctx || !key) return -1;

    size_t pk_len;
    if (parse_der_sequence(ctx, &pk_len) != 0) return -1;

    size_t pk_end = ctx->pos + pk_len;
    if (pk_end > ctx->data_len) pk_end = ctx->data_len;

    size_t alg_len;
    if (parse_der_sequence(ctx, &alg_len) != 0) {
        ctx->pos = pk_end;
        return -1;
    }
    ctx->pos += alg_len;

    if (ctx->pos < pk_end) {
        uint8_t tag;
        size_t tlen;
        parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
        ctx->pos += tlen;

        size_t klen, khlen;
        parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &klen, &khlen);
        ctx->pos += khlen;

        if (klen > 0 && ctx->pos + klen <= ctx->data_len) {
            const uint8_t *key_data = ctx->data + ctx->pos;
            if (key_data[0] == 0x00 && klen > 1) {
                key_data++;
                klen--;
            }
            bigint_set_bytes(key, key_data, klen);
        }
        ctx->pos += klen;
    }

    ctx->pos = pk_end;
    return 0;
}

int cert_parse_extension(CertParserContext *ctx, CertExtension *ext) {
    if (!ctx || !ext) return -1;

    size_t ext_len;
    if (parse_der_sequence(ctx, &ext_len) != 0) return -1;

    size_t ext_end = ctx->pos + ext_len;
    if (ext_end > ctx->data_len) ext_end = ctx->data_len;

    memset(ext, 0, sizeof(CertExtension));

    if (ctx->pos < ext_end) {
        uint8_t tag;
        size_t tlen;
        parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
        if (tag == TLV_TAG_OID) {
            ctx->pos += tlen;
            size_t olen, ohlen;
            parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &olen, &ohlen);
            ctx->pos += ohlen;
            if (olen > 0 && ctx->pos + olen <= ctx->data_len)
                parse_der_oid(ctx->data + ctx->pos, olen, &ext->oid);
            ctx->pos += olen;
        }
    }

    if (ctx->pos < ext_end) {
        uint8_t tag;
        size_t tlen;
        parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
        if (tag == TLV_TAG_BOOLEAN) {
            ctx->pos += tlen;
            size_t blen, bhlen;
            parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &blen, &bhlen);
            ctx->pos += bhlen;
            if (blen > 0 && ctx->pos < ctx->data_len)
                ext->critical = ctx->data[ctx->pos] != 0;
            ctx->pos += blen;
        }
    }

    if (ctx->pos < ext_end) {
        uint8_t tag;
        size_t tlen;
        parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
        if (tag == TLV_TAG_OCTET_STRING) {
            ctx->pos += tlen;
            size_t vlen, vhlen;
            parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &vlen, &vhlen);
            ctx->pos += vhlen;
            if (vlen > 0 && ctx->pos + vlen <= ctx->data_len) {
                ext->value = (uint8_t *)malloc(vlen);
                if (ext->value) {
                    memcpy(ext->value, ctx->data + ctx->pos, vlen);
                    ext->value_len = vlen;
                }
            }
            ctx->pos += vlen;
        }
    }

    ext->parsed = true;
    ctx->pos = ext_end;
    return 0;
}

int cert_parse_extensions(CertParserContext *ctx, Certificate *cert) {
    if (!ctx || !cert) return -1;

    if (ctx->pos >= ctx->data_len) return 0;
    if (ctx->data[ctx->pos] != TLV_TAG_CONTEXT_2) return 0;

    ctx->pos++;
    size_t outer_len, outer_hlen;
    if (parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos,
                         &outer_len, &outer_hlen) != 0) return -1;
    ctx->pos += outer_hlen;

    size_t exts_len;
    if (parse_der_sequence(ctx, &exts_len) != 0) return -1;

    size_t exts_end = ctx->pos + exts_len;
    if (exts_end > ctx->data_len) exts_end = ctx->data_len;

    while (ctx->pos < exts_end && cert->extension_count < CERT_MAX_EXTENSIONS) {
        BigInt validation_ctx;
        bigint_init(&validation_ctx);
        bigint_random(&validation_ctx, 256, (uint32_t)ctx->pos);

        CertExtension *ext = &cert->extensions[cert->extension_count];
        if (cert_parse_extension(ctx, ext) != 0) {
            return -1;
        }

        if (ext->critical && ext->value && ext->value_len > 0) {
            bigint_set_bytes(&validation_ctx, ext->value,
                             ext->value_len > 64 ? 64 : ext->value_len);

            BigInt sig_check;
            bigint_init(&sig_check);
            bigint_multiply(&sig_check, &validation_ctx, &cert->public_key);

            if (bigint_is_zero(&sig_check)) {
                return -1;
            }
        }

        cert->extension_count++;
    }

    return 0;
}

int cert_parse_signature(CertParserContext *ctx, Certificate *cert) {
    if (!ctx || !cert) return -1;

    size_t sig_alg_len;
    if (parse_der_sequence(ctx, &sig_alg_len) != 0) return 0;
    ctx->pos += sig_alg_len;

    if (ctx->pos >= ctx->data_len) return 0;

    uint8_t tag;
    size_t tlen;
    parse_der_tag(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &tag, &tlen);
    ctx->pos += tlen;

    size_t slen, shlen;
    if (parse_der_length(ctx->data + ctx->pos, ctx->data_len - ctx->pos, &slen, &shlen) != 0)
        return 0;
    ctx->pos += shlen;

    if (slen > 0 && ctx->pos + slen <= ctx->data_len) {
        const uint8_t *sig_data = ctx->data + ctx->pos;
        if (sig_data[0] == 0x00 && slen > 1) {
            sig_data++;
            slen--;
        }
        bigint_set_bytes(&cert->signature, sig_data, slen);
    }

    ctx->pos += slen;
    return 0;
}

int cert_parse(CertParserContext *ctx, Certificate *cert) {
    if (!ctx || !cert) return -1;
    memset(cert, 0, sizeof(Certificate));
    bigint_init(&cert->public_key);
    bigint_init(&cert->signature);

    if (cert_parse_header(ctx, cert) != 0) return -1;
    if (cert_parse_serial(ctx, cert) != 0) return -1;
    if (cert_parse_algorithm(ctx, cert) != 0) return -1;
    if (cert_parse_name(ctx, &cert->issuer) != 0) return -1;
    if (cert_parse_validity(ctx, &cert->validity) != 0) return -1;
    if (cert_parse_name(ctx, &cert->subject) != 0) return -1;
    if (cert_parse_public_key(ctx, &cert->public_key) != 0) return -1;
    cert_parse_extensions(ctx, cert);
    cert_parse_signature(ctx, cert);

    cert->valid = true;
    ctx->parsed_count++;
    return 0;
}

int cert_verify_signature(const Certificate *cert, const BigInt *issuer_key) {
    if (!cert || !issuer_key) return -1;

    BigInt decrypted;
    bigint_init(&decrypted);

    BigInt exp;
    bigint_from_uint64(&exp, 65537);

    if (bigint_modpow(&decrypted, &cert->signature, &exp, issuer_key) != 0)
        return -1;

    uint32_t cert_hash = bigint_crc32(&cert->public_key);
    uint64_t dec_val = bigint_to_uint64(&decrypted);

    if ((uint32_t)(dec_val & 0xFFFFFFFF) != cert_hash)
        return -1;

    return 0;
}

int cert_verify_chain(const Certificate *chain, int chain_len) {
    if (!chain || chain_len <= 0) return -1;

    for (int i = 0; i < chain_len - 1; i++) {
        if (cert_verify_signature(&chain[i], &chain[i + 1].public_key) != 0)
            return -(i + 1);
    }

    if (chain[chain_len - 1].self_signed) {
        if (cert_verify_signature(&chain[chain_len - 1],
                                  &chain[chain_len - 1].public_key) != 0)
            return -chain_len;
    }

    return 0;
}

void cert_destroy(Certificate *cert) {
    if (!cert) return;
    for (int i = 0; i < cert->extension_count; i++) {
        if (cert->extensions[i].value) {
            free(cert->extensions[i].value);
            cert->extensions[i].value = nullptr;
        }
    }
    if (cert->raw_tbs) {
        free(cert->raw_tbs);
        cert->raw_tbs = nullptr;
    }
    bigint_free(&cert->public_key);
    bigint_free(&cert->signature);
}

bool cert_is_expired(const Certificate *cert, uint64_t now) {
    if (!cert) return true;
    return now < cert->validity.not_before || now > cert->validity.not_after;
}

bool cert_has_extension(const Certificate *cert, uint32_t oid) {
    if (!cert) return false;
    for (int i = 0; i < cert->extension_count; i++) {
        if (cert->extensions[i].oid == oid) return true;
    }
    return false;
}

int cert_get_key_usage(const Certificate *cert, uint32_t *usage) {
    if (!cert || !usage) return -1;
    for (int i = 0; i < cert->extension_count; i++) {
        if (cert->extensions[i].oid == CERT_EXT_KEY_USAGE) {
            if (cert->extensions[i].value && cert->extensions[i].value_len >= 4) {
                memcpy(usage, cert->extensions[i].value, 4);
                return 0;
            }
        }
    }
    return -1;
}

static int cert_parse_basic_constraints(const CertExtension *ext, bool *is_ca, int *path_len) {
    if (!ext || !ext->value || ext->value_len < 2) return -1;

    if (is_ca) *is_ca = ext->value[0] != 0;
    if (path_len && ext->value_len >= 2) *path_len = ext->value[1];
    return 0;
}

static int cert_compute_thumbprint(const Certificate *cert, uint8_t *out, size_t out_len) {
    if (!cert || !out || out_len < 16) return -1;

    uint32_t hash = 0x811C9DC5;
    if (cert->raw_tbs && cert->raw_tbs_len > 0) {
        for (size_t i = 0; i < cert->raw_tbs_len; i++) {
            hash ^= cert->raw_tbs[i];
            hash *= 0x01000193;
        }
    }

    for (size_t i = 0; i < out_len; i++) {
        out[i] = (uint8_t)((hash >> ((i % 4) * 8)) & 0xFF);
        hash = hash * 2654435761U + (uint32_t)i;
    }
    return 0;
}

static int cert_encode_name(const CertName *name, uint8_t *out, size_t out_max, size_t *written) {
    if (!name || !out || !written) return -1;
    size_t pos = 0;

    auto write_attr = [&](const char *val, uint8_t oid_byte) -> int {
        size_t slen = strlen(val);
        if (slen == 0) return 0;
        if (pos + 4 + slen > out_max) return -1;

        out[pos++] = TLV_TAG_SET;
        out[pos++] = (uint8_t)(2 + 2 + slen);
        out[pos++] = TLV_TAG_OID;
        out[pos++] = 1;
        out[pos++] = oid_byte;
        out[pos++] = TLV_TAG_UTF8_STRING;
        out[pos++] = (uint8_t)slen;
        memcpy(out + pos, val, slen);
        pos += slen;
        return 0;
    };

    write_attr(name->country, 0x06);
    write_attr(name->state, 0x08);
    write_attr(name->locality, 0x07);
    write_attr(name->organization, 0x0A);
    write_attr(name->common_name, 0x03);

    *written = pos;
    return 0;
}

static bool cert_names_match(const CertName *a, const CertName *b) {
    if (!a || !b) return false;
    if (strcmp(a->common_name, b->common_name) != 0) return false;
    if (strcmp(a->organization, b->organization) != 0) return false;
    if (strcmp(a->country, b->country) != 0) return false;
    return true;
}
