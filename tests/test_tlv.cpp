#include <cassert>
#include <cstdio>
#include <cstring>
#include "tlv/tlv_encoder.hpp"
#include "tlv/tlv_decoder.hpp"

int main() {
    uint8_t buffer[1024];
    TLVEncoder enc;
    tlv_encoder_init(&enc, buffer, sizeof(buffer));

    assert(tlv_begin_sequence(&enc) == 0);
    assert(tlv_encode_integer(&enc, 12345) == 0);
    assert(tlv_encode_utf8_string(&enc, "hello") == 0);
    assert(tlv_encode_boolean(&enc, true) == 0);
    assert(tlv_end_sequence(&enc) == 0);

    size_t size = tlv_encoder_size(&enc);
    assert(size > 0);

    TLVDecoder dec;
    tlv_decoder_init(&dec, buffer, size);
    TLVNode *root = nullptr;
    assert(tlv_decode(&dec, &root) == 0);
    assert(root != nullptr);
    assert(tlv_node_count(root) == 4); // Root + 3 children

    tlv_decoder_destroy(&dec);
    tlv_encoder_destroy(&enc);

    printf("test_tlv passed\n");
    return 0;
}
