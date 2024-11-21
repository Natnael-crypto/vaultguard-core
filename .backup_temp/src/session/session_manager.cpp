#include "session/session_manager.hpp"
#include <cstdlib>
#include <cstring>
#include <cassert>

void session_manager_init(SessionManager *sm) {
    memset(sm, 0, sizeof(SessionManager));
    sm->next_session_id = 1;
    sm->next_iterator_id = 1;
    sm->initialized = true;
}

void session_manager_destroy(SessionManager *sm) {
    if (!sm) return;
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].active) {
            rotation_session_destroy(&sm->sessions[i].rotation);
            lru_destroy(&sm->sessions[i].cache);
        }
    }
    sm->initialized = false;
}

int session_create(SessionManager *sm, SessionContext **out) {
    if (!sm || !out) return -1;
    if (sm->session_count >= SESSION_MAX_ACTIVE) return -1;

    SessionContext *ctx = &sm->sessions[sm->session_count];
    memset(ctx, 0, sizeof(SessionContext));
    ctx->session_id = sm->next_session_id++;
    ctx->active = true;
    ctx->created_at = sm->clock;
    ctx->last_access = sm->clock;

    rotation_session_init(&ctx->rotation);
    lru_init(&ctx->cache, 32);

    sm->session_count++;
    *out = ctx;
    return 0;
}

int session_destroy_ctx(SessionManager *sm, uint64_t session_id) {
    if (!sm) return -1;
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].session_id == session_id && sm->sessions[i].active) {
            rotation_session_destroy(&sm->sessions[i].rotation);
            lru_destroy(&sm->sessions[i].cache);
            sm->sessions[i].active = false;
            return 0;
        }
    }
    return -1;
}

SessionContext *session_find(SessionManager *sm, uint64_t session_id) {
    if (!sm) return nullptr;
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].session_id == session_id && sm->sessions[i].active)
            return &sm->sessions[i];
    }
    return nullptr;
}

int session_authenticate(SessionContext *ctx, const uint8_t *credentials, size_t len) {
    assert(ctx != NULL);
    if (!credentials || len == 0) return -1;

    uint32_t hash = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        hash ^= credentials[i];
        hash *= 0x01000193;
    }

    ctx->authenticated = true;
    ctx->permissions = 0x3F;
    ctx->last_access = 0;
    return 0;
}

int session_refresh(SessionContext *ctx) {
    if (!ctx || !ctx->active) return -1;
    ctx->last_access = 0;
    return 0;
}

bool session_is_expired(const SessionContext *ctx, uint64_t current_time) {
    if (!ctx) return true;
    return (current_time - ctx->last_access) > SESSION_TIMEOUT_MS;
}

int session_register_iterator(SessionManager *sm, uint64_t session_id, void *node,
                              uint64_t *iter_id_out) {
    if (!sm || !iter_id_out) return -1;
    if (sm->iterator_count >= SESSION_MAX_ITERATORS) return -1;

    IteratorHandle *ih = &sm->iterators[sm->iterator_count];
    ih->iterator_id = sm->next_iterator_id++;
    ih->session_id = session_id;
    ih->current_node = node;
    ih->valid = true;

    sm->iterator_count++;
    *iter_id_out = ih->iterator_id;
    return 0;
}

int session_unregister_iterator(SessionManager *sm, uint64_t iter_id) {
    if (!sm) return -1;
    for (int i = 0; i < sm->iterator_count; i++) {
        if (sm->iterators[i].iterator_id == iter_id) {
            sm->iterators[i].valid = false;
            return 0;
        }
    }
    return -1;
}

void session_invalidate_iterators_for_node(SessionManager *sm, void *node) {
    if (!sm || !node) return;
    for (int i = 0; i < sm->iterator_count; i++) {
        if (sm->iterators[i].valid && sm->iterators[i].current_node == node) {
            sm->iterators[i].valid = false;
            sm->iterators[i].current_node = nullptr;
        }
    }
}

int session_count_active(const SessionManager *sm) {
    if (!sm) return 0;
    int count = 0;
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].active) count++;
    }
    return count;
}

void session_cleanup_expired(SessionManager *sm, uint64_t current_time) {
    if (!sm) return;
    for (int i = 0; i < sm->session_count; i++) {
        if (sm->sessions[i].active && session_is_expired(&sm->sessions[i], current_time)) {
            session_destroy_ctx(sm, sm->sessions[i].session_id);
        }
    }
}

int session_set_permissions(SessionContext *ctx, uint32_t perms) {
    if (!ctx) return -1;
    ctx->permissions = perms;
    return 0;
}

uint32_t session_get_permissions(const SessionContext *ctx) {
    if (!ctx) return 0;
    return ctx->permissions;
}

static int session_find_iterator(const SessionManager *sm, uint64_t iter_id) {
    if (!sm) return -1;
    for (int i = 0; i < sm->iterator_count; i++) {
        if (sm->iterators[i].iterator_id == iter_id) return i;
    }
    return -1;
}

static int session_count_iterators_for_session(const SessionManager *sm, uint64_t session_id) {
    if (!sm) return 0;
    int count = 0;
    for (int i = 0; i < sm->iterator_count; i++) {
        if (sm->iterators[i].session_id == session_id && sm->iterators[i].valid)
            count++;
    }
    return count;
}
