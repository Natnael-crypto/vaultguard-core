#include <cassert>
#include <cstdio>
#include <cstring>
#include "session/session_manager.hpp"
#include "session/lru_cache.hpp"
#include "session/key_rotation.hpp"

void test_lru() {
    LRUCache cache;
    lru_init(&cache, 10);

    char *val1 = strdup("value1");
    char *val2 = strdup("value2");

    assert(lru_put(&cache, "key1", val1, 7) == 0);
    assert(lru_put(&cache, "key2", val2, 7) == 0);

    assert(lru_get(&cache, "key1") == val1);
    assert(lru_count(&cache) == 2);

    assert(lru_remove(&cache, "key1") == 0);
    assert(lru_count(&cache) == 1);

    lru_destroy(&cache);
}

void test_session_mgr() {
    SessionManager sm;
    session_manager_init(&sm);

    SessionContext *ctx = nullptr;
    assert(session_create(&sm, &ctx) == 0);
    assert(ctx != nullptr);
    assert(session_count_active(&sm) == 1);

    uint8_t creds[] = "test";
    assert(session_authenticate(ctx, creds, 4) == 0);

    assert(session_destroy_ctx(&sm, ctx->session_id) == 0);
    assert(session_count_active(&sm) == 0);

    session_manager_destroy(&sm);
}

int main() {
    test_lru();
    test_session_mgr();
    printf("test_session passed\n");
    return 0;
}
