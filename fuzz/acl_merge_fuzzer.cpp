#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "vault/acl_engine.hpp"
#include "crypto/bigint.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    uint8_t mode = data[0];
    const uint8_t *payload = data + 2;
    size_t payload_len = size - 2;

    if ((mode % 3) == 0) {
        size_t entry_count = payload[0] % 16;
        if (entry_count == 0) entry_count = 1;
        if (entry_count * 4 > payload_len) entry_count = payload_len / 4;

        ACLEntry *entries = (ACLEntry *)calloc(entry_count, sizeof(ACLEntry));
        if (!entries) return 0;

        for (size_t i = 0; i < entry_count; i++) {
            size_t off = 1 + i * 4;
            if (off + 4 > payload_len) break;
            entries[i].subject_id = payload[off];
            entries[i].permissions = payload[off + 1] | ((uint32_t)payload[off + 2] << 8);
            entries[i].effect = (payload[off + 3] & 1) ? ACL_EFFECT_ALLOW : ACL_EFFECT_DENY;
            entries[i].priority = payload[off + 3] >> 1;
            entries[i].active = true;
            snprintf(entries[i].resource, sizeof(entries[i].resource), "/res/%zu", i);
        }

        uint32_t *output = (uint32_t *)calloc(entry_count + 8, sizeof(uint32_t));
        if (output) {
            size_t out_count = 0;
            acl_merge_entries_unrolled(entries, entry_count, output, &out_count);
            free(output);
        }

        free(entries);
    }

    if ((mode % 3) == 1) {
        ACLRuleset rs_a, rs_b, merged;
        acl_ruleset_init(&rs_a);
        acl_ruleset_init(&rs_b);

        int num_rules = (int)(payload[0] % 8) + 1;
        for (int i = 0; i < num_rules && (size_t)(i * 3 + 1) < payload_len; i++) {
            ACLEntry e;
            memset(&e, 0, sizeof(e));
            e.subject_id = payload[i * 3 + 1];
            e.permissions = payload[i * 3 + 2] & 0x3F;
            e.effect = (payload[i * 3 + 2] & 0x40) ? ACL_EFFECT_ALLOW : ACL_EFFECT_DENY;
            e.priority = i;
            snprintf(e.resource, sizeof(e.resource), "/vault/key_%d", i);
            e.match_type = ACL_MATCH_EXACT;
            e.active = true;

            if (i % 2 == 0) acl_add_rule(&rs_a, &e);
            else acl_add_rule(&rs_b, &e);
        }

        acl_merge_rulesets(&rs_a, &rs_b, &merged);

        if (payload_len > 4) {
            acl_check(&merged, payload[1], "/vault/key_0", payload[2] & 0x3F);
        }

        acl_ruleset_destroy(&rs_a);
        acl_ruleset_destroy(&rs_b);
        acl_ruleset_destroy(&merged);
    }

    if ((mode % 3) == 2) {
        BigInt a, b, result;
        bigint_init(&a);
        bigint_init(&b);
        bigint_init(&result);

        size_t half = payload_len / 2;
        if (half > 0 && half <= 128) {
            bigint_set_bytes(&a, payload, half);
            bigint_set_bytes(&b, payload + half, payload_len - half);

            a.flags = payload[0];

            bigint_mul_optimized(&result, &a, &b);

            bigint_add(&result, &a, &b);
            bigint_subtract(&result, &a, &b);

            if (!bigint_is_zero(&b)) {
                BigInt q, r;
                bigint_init(&q);
                bigint_init(&r);
                bigint_divide(&q, &r, &a, &b);
            }

            bigint_gcd(&result, &a, &b);
        }

        bigint_free(&a);
        bigint_free(&b);
        bigint_free(&result);
    }

    return 0;
}
