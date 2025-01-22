#ifndef VAULTGUARD_TLV_DECODER_HPP
#define VAULTGUARD_TLV_DECODER_HPP

#include <cstddef>
#include <cstdint>

#define TLV_TAG_INTEGER      0x02
#define TLV_TAG_BITSTRING    0x03
#define TLV_TAG_OCTET_STRING 0x04
#define TLV_TAG_NULL         0x05
#define TLV_TAG_OID          0x06
#define TLV_TAG_UTF8_STRING  0x0C
#define TLV_TAG_BOOLEAN      0x01
#define TLV_TAG_SEQUENCE     0x30
#define TLV_TAG_SET          0x31
#define TLV_INDEFINITE_LENGTH 0x80
#define TLV_MAX_DEPTH        32
#define TLV_MAX_CHILDREN     256

typedef struct TLVNode TLVNode;

struct TLVNode {
    uint8_t  tag;
    size_t   length;
    const uint8_t *value;
    bool     constructed;
    TLVNode *children;
    int      child_count;
    int      child_capacity;
    TLVNode *parent;
    size_t   header_size;
    size_t   total_size;
};

struct TLVDecoder {
    const uint8_t *data;
    size_t   data_len;
    size_t   pos;
    uint32_t current_depth;
    struct { TLVNode *node; size_t remaining; int child_index; } parent_stack[TLV_MAX_DEPTH];
    TLVNode *root;
    int      node_count;
    int      max_nodes;
    bool     strict;
    char     error_msg[256];
};

void tlv_decoder_init(TLVDecoder *dec, const uint8_t *data, size_t len);
void tlv_decoder_destroy(TLVDecoder *dec);
void tlv_node_init(TLVNode *node);
void tlv_node_destroy(TLVNode *node);
int  tlv_decode(TLVDecoder *dec, TLVNode **root);
int  tlv_node_add_child(TLVNode *parent, TLVNode *child);
TLVNode *tlv_find_tag(const TLVNode *root, uint8_t tag);
TLVNode *tlv_find_tag_recursive(const TLVNode *root, uint8_t tag);
int  tlv_node_count(const TLVNode *root);
int  tlv_get_integer(const TLVNode *node, int64_t *out);
int  tlv_get_bytes(const TLVNode *node, uint8_t *out, size_t out_max, size_t *written);
int  tlv_get_string(const TLVNode *node, char *out, size_t out_max);
void tlv_dump(const TLVNode *root, int indent);

#endif
