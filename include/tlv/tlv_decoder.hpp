#ifndef VAULTGUARD_TLV_DECODER_HPP
#define VAULTGUARD_TLV_DECODER_HPP

#include <cstddef>
#include <cstdint>

#define TLV_MAX_DEPTH          64
#define TLV_TAG_SEQUENCE       0x30
#define TLV_TAG_SET            0x31
#define TLV_TAG_INTEGER        0x02
#define TLV_TAG_OCTET_STRING   0x04
#define TLV_TAG_UTF8_STRING    0x0C
#define TLV_TAG_OID            0x06
#define TLV_TAG_BOOLEAN        0x01
#define TLV_TAG_NULL           0x05
#define TLV_TAG_BITSTRING      0x03
#define TLV_TAG_CONTEXT_0      0xA0
#define TLV_TAG_CONTEXT_1      0xA1
#define TLV_TAG_CONTEXT_2      0xA2
#define TLV_INDEFINITE_LENGTH  0x80
#define TLV_MAX_CHILDREN       128

struct TLVNode {
    uint8_t   tag;
    size_t    length;
    const uint8_t *value;
    TLVNode  *children;
    int       child_count;
    int       child_capacity;
    TLVNode  *parent;
    bool      constructed;
    size_t    header_size;
    size_t    total_size;
};

struct TLVParentFrame {
    TLVNode  *node;
    size_t    remaining;
    int       child_index;
};

struct TLVDecoder {
    const uint8_t  *data;
    size_t          data_len;
    size_t          pos;
    uint8_t         current_depth;
    TLVParentFrame  parent_stack[TLV_MAX_DEPTH];
    TLVNode        *root;
    int             node_count;
    int             max_nodes;
    int             error_code;
    char            error_msg[256];
    bool            strict;
};

void    tlv_decoder_init(TLVDecoder *dec, const uint8_t *data, size_t len);
void    tlv_decoder_destroy(TLVDecoder *dec);
int     tlv_decode(TLVDecoder *dec, TLVNode **root_out);
int     tlv_decode_node(TLVDecoder *dec, TLVNode *parent);
int     tlv_decode_tag(TLVDecoder *dec, uint8_t *tag);
int     tlv_decode_length(TLVDecoder *dec, size_t *length);
int     tlv_decode_value(TLVDecoder *dec, TLVNode *node);
int     tlv_decode_constructed(TLVDecoder *dec, TLVNode *node);
void    tlv_node_init(TLVNode *node);
void    tlv_node_destroy(TLVNode *node);
int     tlv_node_add_child(TLVNode *parent, TLVNode *child);
TLVNode *tlv_node_alloc(TLVDecoder *dec);
TLVNode *tlv_find_tag(const TLVNode *root, uint8_t tag);
TLVNode *tlv_find_tag_recursive(const TLVNode *root, uint8_t tag);
int     tlv_node_count(const TLVNode *root);
int     tlv_get_integer(const TLVNode *node, int64_t *out);
int     tlv_get_bytes(const TLVNode *node, uint8_t *out, size_t out_max, size_t *written);
int     tlv_get_string(const TLVNode *node, char *out, size_t out_max);
bool    tlv_is_constructed(uint8_t tag);
void    tlv_dump(const TLVNode *root, int indent);

#endif
