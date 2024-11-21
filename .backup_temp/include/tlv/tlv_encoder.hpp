#ifndef VAULTGUARD_TLV_ENCODER_HPP
#define VAULTGUARD_TLV_ENCODER_HPP

#include <cstddef>
#include <cstdint>
#include "tlv/tlv_decoder.hpp"

#define TLV_ENCODE_BUF_SIZE    8192

struct TLVEncoder {
    uint8_t *buffer;
    size_t   capacity;
    size_t   pos;
    int      depth;
    size_t   length_offsets[TLV_MAX_DEPTH];
    bool     owns_buffer;
    int      error_code;
};

void    tlv_encoder_init(TLVEncoder *enc, uint8_t *buffer, size_t capacity);
void    tlv_encoder_init_alloc(TLVEncoder *enc, size_t initial_capacity);
void    tlv_encoder_destroy(TLVEncoder *enc);
int     tlv_encode_tag(TLVEncoder *enc, uint8_t tag);
int     tlv_encode_length(TLVEncoder *enc, size_t length);
int     tlv_encode_integer(TLVEncoder *enc, int64_t value);
int     tlv_encode_octet_string(TLVEncoder *enc, const uint8_t *data, size_t len);
int     tlv_encode_utf8_string(TLVEncoder *enc, const char *str);
int     tlv_encode_boolean(TLVEncoder *enc, bool value);
int     tlv_encode_null(TLVEncoder *enc);
int     tlv_encode_oid(TLVEncoder *enc, uint32_t oid);
int     tlv_encode_bitstring(TLVEncoder *enc, const uint8_t *data, size_t bit_count);
int     tlv_begin_sequence(TLVEncoder *enc);
int     tlv_end_sequence(TLVEncoder *enc);
int     tlv_begin_set(TLVEncoder *enc);
int     tlv_end_set(TLVEncoder *enc);
int     tlv_begin_context(TLVEncoder *enc, uint8_t context_tag);
int     tlv_end_context(TLVEncoder *enc);
int     tlv_encode_node(TLVEncoder *enc, const TLVNode *node);
int     tlv_encode_tree(TLVEncoder *enc, const TLVNode *root);
size_t  tlv_encoder_size(const TLVEncoder *enc);
const uint8_t *tlv_encoder_data(const TLVEncoder *enc);
int     tlv_encoder_reset(TLVEncoder *enc);
int     tlv_encode_raw(TLVEncoder *enc, const uint8_t *data, size_t len);

#endif
