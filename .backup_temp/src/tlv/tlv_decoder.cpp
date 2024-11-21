//
// tlv_decoder.cpp
//

#include "tlv/tlv_decoder.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

void tlv_decoder_init(TLVDecoder *dec, const uint8_t *data, size_t len) {
    memset(dec, 0, sizeof(TLVDecoder));
    dec->data = data;
    dec->data_len = len;
    dec->pos = 0;
    dec->current_depth = 0;
    dec->root = nullptr;
    dec->max_nodes = 4096;
    dec->strict = true;
}

void tlv_node_init(TLVNode *node) {
    memset(node, 0, sizeof(TLVNode));
    node->children = nullptr;
    node->child_count = 0;
    node->child_capacity = 0;
    node->parent = nullptr;
}

void tlv_node_destroy(TLVNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        tlv_node_destroy(&node->children[i]);
    }
    if (node->children) {
        free(node->children);
        node->children = nullptr;
    }
    node->child_count = 0;
    node->child_capacity = 0;
}

void tlv_decoder_destroy(TLVDecoder *dec) {
    if (!dec) return;
    if (dec->root) {
        tlv_node_destroy(dec->root);
        free(dec->root);
        dec->root = nullptr;
    }
}

TLVNode *tlv_node_alloc(TLVDecoder *dec) {
    if (!dec) return nullptr;
    if (dec->node_count >= dec->max_nodes) return nullptr;

    TLVNode *node = (TLVNode *)malloc(sizeof(TLVNode));
    if (!node) return nullptr;

    tlv_node_init(node);
    dec->node_count++;
    return node;
}

int tlv_node_add_child(TLVNode *parent, TLVNode *child) {
    if (!parent || !child) return -1;

    if (parent->child_count >= parent->child_capacity) {
        int new_cap = parent->child_capacity == 0 ? 8 : parent->child_capacity * 2;
        if (new_cap > TLV_MAX_CHILDREN) new_cap = TLV_MAX_CHILDREN;
        if (parent->child_count >= new_cap) return -1;

        TLVNode *new_children = (TLVNode *)realloc(parent->children,
                                                    (size_t)new_cap * sizeof(TLVNode));
        if (!new_children) return -1;

        parent->children = new_children;
        parent->child_capacity = new_cap;
    }

    memcpy(&parent->children[parent->child_count], child, sizeof(TLVNode));
    parent->children[parent->child_count].parent = parent;
    parent->child_count++;
    return 0;
}

bool tlv_is_constructed(uint8_t tag) {
    return (tag & 0x20) != 0 || tag == TLV_TAG_SEQUENCE || tag == TLV_TAG_SET;
}

int tlv_decode_tag(TLVDecoder *dec, uint8_t *tag) {
    if (!dec || !tag) return -1;
    if (dec->pos >= dec->data_len) return -1;
    *tag = dec->data[dec->pos];
    dec->pos++;
    return 0;
}

int tlv_decode_length(TLVDecoder *dec, size_t *length) {
    if (!dec || !length) return -1;
    if (dec->pos >= dec->data_len) return -1;

    uint8_t first = dec->data[dec->pos];
    dec->pos++;

    if (first < 0x80) {
        *length = first;
        return 0;
    }

    if (first == TLV_INDEFINITE_LENGTH) {
        *length = 0;
        return 0;
    }

    int num_bytes = first & 0x7F;
    if (num_bytes > 4) {
        snprintf(dec->error_msg, sizeof(dec->error_msg), "length too large");
        return -1;
    }

    if (dec->pos + (size_t)num_bytes > dec->data_len) return -1;

    size_t val = 0;
    for (int i = 0; i < num_bytes; i++) {
        val = (val << 8) | dec->data[dec->pos++];
    }
    *length = val;
    return 0;
}

int tlv_decode_constructed(TLVDecoder *dec, TLVNode *node) {
    if (!dec || !node) return -1;

    size_t end_pos = dec->pos + node->length;
    if (end_pos > dec->data_len) end_pos = dec->data_len;

    dec->current_depth++;

    dec->parent_stack[dec->current_depth].node = node;
    dec->parent_stack[dec->current_depth].remaining = node->length;
    dec->parent_stack[dec->current_depth].child_index = 0;

    while (dec->pos < end_pos) {
        TLVNode child;
        tlv_node_init(&child);

        if (tlv_decode_tag(dec, &child.tag) != 0) break;

        if (tlv_decode_length(dec, &child.length) != 0) break;

        child.constructed = tlv_is_constructed(child.tag);
        child.value = dec->data + dec->pos;
        child.header_size = 2;
        child.total_size = child.header_size + child.length;

        if (child.constructed && child.length > 0) {
            if (tlv_decode_constructed(dec, &child) != 0) break;
        } else {
            if (dec->pos + child.length <= dec->data_len) {
                dec->pos += child.length;
            } else {
                dec->pos = dec->data_len;
            }
        }

        tlv_node_add_child(node, &child);
    }

    dec->current_depth--;
    return 0;
}

int tlv_decode_node(TLVDecoder *dec, TLVNode *parent) {
    if (!dec || !parent) return -1;

    while (dec->pos < dec->data_len) {
        TLVNode child;
        tlv_node_init(&child);

        if (tlv_decode_tag(dec, &child.tag) != 0) return -1;
        if (tlv_decode_length(dec, &child.length) != 0) return -1;

        child.constructed = tlv_is_constructed(child.tag);
        child.value = dec->data + dec->pos;
        child.header_size = 2;
        child.total_size = child.header_size + child.length;

        if (child.constructed && child.length > 0) {
            tlv_decode_constructed(dec, &child);
        } else {
            dec->pos += child.length;
        }

        tlv_node_add_child(parent, &child);
    }
    return 0;
}

int tlv_decode(TLVDecoder *dec, TLVNode **root_out) {
    if (!dec || !root_out) return -1;

    dec->root = tlv_node_alloc(dec);
    if (!dec->root) return -1;

    dec->root->tag = 0xFF;
    dec->root->constructed = true;

    dec->current_depth = 0;
    dec->parent_stack[0].node = dec->root;
    dec->parent_stack[0].remaining = dec->data_len;
    dec->parent_stack[0].child_index = 0;

    if (tlv_decode_node(dec, dec->root) != 0) {
        *root_out = dec->root;
        return -1;
    }

    *root_out = dec->root;
    return 0;
}

int tlv_decode_value(TLVDecoder *dec, TLVNode *node) {
    if (!dec || !node) return -1;
    if (dec->pos + node->length > dec->data_len) return -1;
    node->value = dec->data + dec->pos;
    dec->pos += node->length;
    return 0;
}

TLVNode *tlv_find_tag(const TLVNode *root, uint8_t tag) {
    if (!root) return nullptr;
    for (int i = 0; i < root->child_count; i++) {
        if (root->children[i].tag == tag)
            return &root->children[i];
    }
    return nullptr;
}

TLVNode *tlv_find_tag_recursive(const TLVNode *root, uint8_t tag) {
    if (!root) return nullptr;
    for (int i = 0; i < root->child_count; i++) {
        if (root->children[i].tag == tag)
            return &root->children[i];
        TLVNode *found = tlv_find_tag_recursive(&root->children[i], tag);
        if (found) return found;
    }
    return nullptr;
}

int tlv_node_count(const TLVNode *root) {
    if (!root) return 0;
    int count = 1;
    for (int i = 0; i < root->child_count; i++) {
        count += tlv_node_count(&root->children[i]);
    }
    return count;
}

int tlv_get_integer(const TLVNode *node, int64_t *out) {
    if (!node || !out) return -1;
    if (node->tag != TLV_TAG_INTEGER) return -1;
    if (!node->value || node->length == 0) return -1;

    int64_t val = 0;
    bool negative = (node->value[0] & 0x80) != 0;

    for (size_t i = 0; i < node->length; i++) {
        val = (val << 8) | node->value[i];
    }

    if (negative && node->length < 8) {
        int shift = (int)(64 - node->length * 8);
        val = (val << shift) >> shift;
    }

    *out = val;
    return 0;
}

int tlv_get_bytes(const TLVNode *node, uint8_t *out, size_t out_max, size_t *written) {
    if (!node || !out || !written) return -1;
    if (!node->value) return -1;

    size_t copy = node->length < out_max ? node->length : out_max;
    memcpy(out, node->value, copy);
    *written = copy;
    return 0;
}

int tlv_get_string(const TLVNode *node, char *out, size_t out_max) {
    if (!node || !out) return -1;
    if (!node->value) return -1;

    size_t copy = node->length < out_max - 1 ? node->length : out_max - 1;
    memcpy(out, node->value, copy);
    out[copy] = '\0';
    return 0;
}

void tlv_dump(const TLVNode *root, int indent) {
    if (!root) return;
    for (int i = 0; i < indent; i++) fprintf(stderr, "  ");
    fprintf(stderr, "Tag=0x%02X Len=%zu Children=%d\n",
            root->tag, root->length, root->child_count);
    for (int i = 0; i < root->child_count; i++) {
        tlv_dump(&root->children[i], indent + 1);
    }
}

static int tlv_validate_structure(const TLVNode *node, int max_depth, int cur_depth) {
    if (!node) return 0;
    if (cur_depth > max_depth) return -1;

    for (int i = 0; i < node->child_count; i++) {
        if (tlv_validate_structure(&node->children[i], max_depth, cur_depth + 1) != 0)
            return -1;
    }
    return 0;
}

static size_t tlv_total_size(const TLVNode *node) {
    if (!node) return 0;
    size_t total = node->total_size;
    for (int i = 0; i < node->child_count; i++) {
        total += tlv_total_size(&node->children[i]);
    }
    return total;
}

static int tlv_flatten(const TLVNode *root, TLVNode **out, int max_nodes, int *count) {
    if (!root || !out || !count) return -1;
    if (*count >= max_nodes) return -1;

    out[*count] = (TLVNode *)root;
    (*count)++;

    for (int i = 0; i < root->child_count; i++) {
        if (tlv_flatten(&root->children[i], out, max_nodes, count) != 0)
            return -1;
    }
    return 0;
}
