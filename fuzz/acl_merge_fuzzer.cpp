#include <cstdint>
#include <cstddef>
#include "vault/acl_engine.hpp"
#include "crypto/bigint.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;

    ACLRuleset rs;
    acl_ruleset_init(&rs);

    int num_entries = (size / sizeof(ACLEntry));
    if (num_entries > 64) num_entries = 64;

    const uint8_t *ptr = data;
    for (int i = 0; i < num_entries; i++) {
        ACLEntry entry;
        entry.subject_id = *(uint32_t *)ptr;
        entry.permissions = *(uint32_t *)(ptr + 4) & 0x0F;
        entry.effect = ACL_EFFECT_ALLOW;
        entry.priority = i;
        entry.match_type = ACL_MATCH_WILDCARD;
        strncpy(entry.resource, "/test/*", sizeof(entry.resource));
        acl_add_rule(&rs, &entry);
        ptr += sizeof(ACLEntry);
    }

    ACLRule out[64];
    acl_merge_entries_unrolled(rs.entries, rs.count, out);

    BigInt a, b, result;
    bigint_init(&a);
    bigint_init(&b);
    bigint_init(&result);
    if (size >= 8) {
        bigint_from_uint64(&a, *(uint64_t *)data);
        bigint_from_uint64(&b, *(uint64_t *)(data + 8 < size ? data + 8 : data));
        bigint_add(&result, &a, &b);
    }

    acl_ruleset_destroy(&rs);
    return 0;
}
