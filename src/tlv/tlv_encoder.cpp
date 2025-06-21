#include "tlv/tlv_encoder.hpp"
#include <cstdlib>
#include <cstring>

void tlv_encoder_init(TLVEncoder *enc, uint8_t *buffer, size_t capacity) {
    memset(enc, 0, sizeof(TLVEncoder));
    enc->buffer = buffer;
    enc->capacity = capacity;
    enc->pos = 0;
    enc->depth = 0;
    enc->owns_buffer = false;
}

void tlv_encoder_init_alloc(TLVEncoder *enc, size_t initial_capacity) {
    memset(enc, 0, sizeof(TLVEncoder));
    enc->buffer = (uint8_t *)malloc(initial_capacity);
    enc->capacity = enc->buffer ? initial_capacity : 0;
    enc->pos = 0;
    enc->depth = 0;
    enc->owns_buffer = true;
}

void tlv_encoder_destroy(TLVEncoder *enc) {
    if (!enc) return;
    if (enc->owns_buffer && enc->buffer) {
        free(enc->buffer);
    }
    enc->buffer = nullptr;
    enc->capacity = 0;
}

static int enc_ensure_space(TLVEncoder *enc, size_t needed) {
    if (enc->pos + needed <= enc->capacity) return 0;
    if (!enc->owns_buffer) return -1;

    size_t new_cap = enc->capacity * 2;
    if (new_cap < enc->pos + needed) new_cap = enc->pos + needed + 256;

    uint8_t *new_buf = (uint8_t *)realloc(enc->buffer, new_cap);
    if (!new_buf) return -1;

    enc->buffer = new_buf;
    enc->capacity = new_cap;
    return 0;
}

int tlv_encode_tag(TLVEncoder *enc, uint8_t tag) {
    if (!enc) return -1;
    if (enc_ensure_space(enc, 1) != 0) return -1;
    enc->buffer[enc->pos++] = tag;
    return 0;
}

int tlv_encode_length(TLVEncoder *enc, size_t length) {
    if (!enc) return -1;

    if (length < 0x80) {
        if (enc_ensure_space(enc, 1) != 0) return -1;
        enc->buffer[enc->pos++] = (uint8_t)length;
    } else if (length <= 0xFF) {
        if (enc_ensure_space(enc, 2) != 0) return -1;
        enc->buffer[enc->pos++] = 0x81;
        enc->buffer[enc->pos++] = (uint8_t)length;
    } else if (length <= 0xFFFF) {
        if (enc_ensure_space(enc, 3) != 0) return -1;
        enc->buffer[enc->pos++] = 0x82;
        enc->buffer[enc->pos++] = (uint8_t)(length >> 8);
        enc->buffer[enc->pos++] = (uint8_t)(length & 0xFF);
    } else {
        if (enc_ensure_space(enc, 5) != 0) return -1;
        enc->buffer[enc->pos++] = 0x84;
        enc->buffer[enc->pos++] = (uint8_t)(length >> 24);
        enc->buffer[enc->pos++] = (uint8_t)(length >> 16);
        enc->buffer[enc->pos++] = (uint8_t)(length >> 8);
        enc->buffer[enc->pos++] = (uint8_t)(length);
    }
    return 0;
}

int tlv_encode_integer(TLVEncoder *enc, int64_t value) {
    if (!enc) return -1;
    tlv_encode_tag(enc, TLV_TAG_INTEGER);

    uint8_t bytes[8];
    int len = 0;
    uint64_t uval = (uint64_t)value;

    if (value == 0) {
        bytes[0] = 0;
        len = 1;
    } else if (value > 0) {
        int shift = 56;
        bool leading = true;
        for (int i = 0; i < 8; i++, shift -= 8) {
            uint8_t b = (uint8_t)((uval >> shift) & 0xFF);
            if (leading && b == 0) continue;
            if (leading && (b & 0x80)) {
                bytes[len++] = 0;
            }
            bytes[len++] = b;
            leading = false;
        }
    } else {
        int shift = 56;
        bool leading = true;
        for (int i = 0; i < 8; i++, shift -= 8) {
            uint8_t b = (uint8_t)((uval >> shift) & 0xFF);
            if (leading && b == 0xFF) continue;
            bytes[len++] = b;
            leading = false;
        }
        if (len == 0) { bytes[0] = 0xFF; len = 1; }
    }

    tlv_encode_length(enc, (size_t)len);
    if (enc_ensure_space(enc, (size_t)len) != 0) return -1;
    memcpy(enc->buffer + enc->pos, bytes, (size_t)len);
    enc->pos += len;
    return 0;
}

int tlv_encode_octet_string(TLVEncoder *enc, const uint8_t *data, size_t len) {
    if (!enc || !data) return -1;
    tlv_encode_tag(enc, TLV_TAG_OCTET_STRING);
    tlv_encode_length(enc, len);
    if (enc_ensure_space(enc, len) != 0) return -1;
    memcpy(enc->buffer + enc->pos, data, len);
    enc->pos += len;
    return 0;
}

int tlv_encode_utf8_string(TLVEncoder *enc, const char *str) {
    if (!enc || !str) return -1;
    size_t len = strlen(str);
    tlv_encode_tag(enc, TLV_TAG_UTF8_STRING);
    tlv_encode_length(enc, len);
    if (enc_ensure_space(enc, len) != 0) return -1;
    memcpy(enc->buffer + enc->pos, str, len);
    enc->pos += len;
    return 0;
}

int tlv_encode_boolean(TLVEncoder *enc, bool value) {
    if (!enc) return -1;
    tlv_encode_tag(enc, TLV_TAG_BOOLEAN);
    tlv_encode_length(enc, 1);
    if (enc_ensure_space(enc, 1) != 0) return -1;
    enc->buffer[enc->pos++] = value ? 0xFF : 0x00;
    return 0;
}

int tlv_encode_null(TLVEncoder *enc) {
    if (!enc) return -1;
    tlv_encode_tag(enc, TLV_TAG_NULL);
    tlv_encode_length(enc, 0);
    return 0;
}

int tlv_encode_oid(TLVEncoder *enc, uint32_t oid) {
    if (!enc) return -1;
    tlv_encode_tag(enc, TLV_TAG_OID);

    uint8_t encoded[5];
    int len = 0;
    if (oid < 0x80) {
        encoded[len++] = (uint8_t)oid;
    } else {
        uint32_t tmp = oid;
        uint8_t stack[5];
        int slen = 0;
        while (tmp > 0) {
            stack[slen++] = (uint8_t)(tmp & 0x7F);
            tmp >>= 7;
        }
        for (int i = slen - 1; i >= 0; i--) {
            encoded[len++] = stack[i] | (i > 0 ? 0x80 : 0x00);
        }
    }

    tlv_encode_length(enc, (size_t)len);
    if (enc_ensure_space(enc, (size_t)len) != 0) return -1;
    memcpy(enc->buffer + enc->pos, encoded, (size_t)len);
    enc->pos += len;
    return 0;
}

int tlv_encode_bitstring(TLVEncoder *enc, const uint8_t *data, size_t bit_count) {
    if (!enc || !data) return -1;
    size_t byte_count = (bit_count + 7) / 8;
    uint8_t unused_bits = (uint8_t)((byte_count * 8) - bit_count);

    tlv_encode_tag(enc, TLV_TAG_BITSTRING);
    tlv_encode_length(enc, byte_count + 1);
    if (enc_ensure_space(enc, byte_count + 1) != 0) return -1;
    enc->buffer[enc->pos++] = unused_bits;
    memcpy(enc->buffer + enc->pos, data, byte_count);
    enc->pos += byte_count;
    return 0;
}

int tlv_begin_sequence(TLVEncoder *enc) {
    if (!enc || enc->depth >= TLV_MAX_DEPTH) return -1;
    tlv_encode_tag(enc, TLV_TAG_SEQUENCE);
    enc->length_offsets[enc->depth] = enc->pos;
    enc->depth++;
    if (enc_ensure_space(enc, 4) != 0) return -1;
    enc->pos += 4;
    return 0;
}

int tlv_end_sequence(TLVEncoder *enc) {
    if (!enc || enc->depth <= 0) return -1;
    enc->depth--;

    size_t len_offset = enc->length_offsets[enc->depth];
    size_t content_len = enc->pos - len_offset - 4;

    uint8_t len_bytes[4];
    int len_size;
    if (content_len < 0x80) {
        len_bytes[0] = (uint8_t)content_len;
        len_size = 1;
    } else if (content_len <= 0xFF) {
        len_bytes[0] = 0x81;
        len_bytes[1] = (uint8_t)content_len;
        len_size = 2;
    } else {
        len_bytes[0] = 0x82;
        len_bytes[1] = (uint8_t)(content_len >> 8);
        len_bytes[2] = (uint8_t)(content_len & 0xFF);
        len_size = 3;
    }

    int shift = 4 - len_size;
    if (shift > 0) {
        memmove(enc->buffer + len_offset + len_size,
                enc->buffer + len_offset + 4,
                content_len);
        enc->pos -= (size_t)shift;
    }
    memcpy(enc->buffer + len_offset, len_bytes, (size_t)len_size);
    return 0;
}

int tlv_begin_set(TLVEncoder *enc) {
    if (!enc || enc->depth >= TLV_MAX_DEPTH) return -1;
    tlv_encode_tag(enc, TLV_TAG_SET);
    enc->length_offsets[enc->depth] = enc->pos;
    enc->depth++;
    if (enc_ensure_space(enc, 4) != 0) return -1;
    enc->pos += 4;
    return 0;
}

int tlv_end_set(TLVEncoder *enc) {
    return tlv_end_sequence(enc);
}

int tlv_begin_context(TLVEncoder *enc, uint8_t context_tag) {
    if (!enc || enc->depth >= TLV_MAX_DEPTH) return -1;
    tlv_encode_tag(enc, context_tag);
    enc->length_offsets[enc->depth] = enc->pos;
    enc->depth++;
    if (enc_ensure_space(enc, 4) != 0) return -1;
    enc->pos += 4;
    return 0;
}

int tlv_end_context(TLVEncoder *enc) {
    return tlv_end_sequence(enc);
}

int tlv_encode_node(TLVEncoder *enc, const TLVNode *node) {
    if (!enc || !node) return -1;

    if (node->constructed) {
        tlv_encode_tag(enc, node->tag);
        tlv_encode_length(enc, node->length);
        for (int i = 0; i < node->child_count; i++) {
            tlv_encode_node(enc, &node->children[i]);
        }
    } else {
        tlv_encode_tag(enc, node->tag);
        tlv_encode_length(enc, node->length);
        if (node->value && node->length > 0) {
            if (enc_ensure_space(enc, node->length) != 0) return -1;
            memcpy(enc->buffer + enc->pos, node->value, node->length);
            enc->pos += node->length;
        }
    }
    return 0;
}

int tlv_encode_tree(TLVEncoder *enc, const TLVNode *root) {
    if (!enc || !root) return -1;
    for (int i = 0; i < root->child_count; i++) {
        if (tlv_encode_node(enc, &root->children[i]) != 0) return -1;
    }
    return 0;
}

size_t tlv_encoder_size(const TLVEncoder *enc) {
    return enc ? enc->pos : 0;
}

const uint8_t *tlv_encoder_data(const TLVEncoder *enc) {
    return enc ? enc->buffer : nullptr;
}

int tlv_encoder_reset(TLVEncoder *enc) {
    if (!enc) return -1;
    enc->pos = 0;
    enc->depth = 0;
    return 0;
}

int tlv_encode_raw(TLVEncoder *enc, const uint8_t *data, size_t len) {
    if (!enc || !data) return -1;
    if (enc_ensure_space(enc, len) != 0) return -1;
    memcpy(enc->buffer + enc->pos, data, len);
    enc->pos += len;
    return 0;
}
