#include <cstdint>
#include <cstddef>
#include "tlv/tlv_decoder.hpp"
#include "vault/vault_tree.hpp"
#include "allocator/secure_arena.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;

    TLVDecoder dec;
    tlv_decoder_init(&dec, data, size);
    TLVNode *root = nullptr;
    tlv_decode(&dec, &root);

    if (root) {
        tlv_node_count(root);
        tlv_find_tag(root, 0x02);
        tlv_dump(root, 0);
    }

    tlv_decoder_destroy(&dec);
    return 0;
}
