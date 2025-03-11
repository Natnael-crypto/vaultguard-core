#ifndef VAULTGUARD_SESSION_MANAGER_HPP
#define VAULTGUARD_SESSION_MANAGER_HPP

#include <cstddef>
#include <cstdint>
#include "session/key_rotation.hpp"
#include "session/lru_cache.hpp"

#define SESSION_MAX_ACTIVE     64
#define SESSION_TIMEOUT_MS     300000
#define SESSION_MAX_ITERATORS  128

struct IteratorHandle {
    uint64_t iterator_id;
    uint64_t session_id;
    void    *current_node;
    bool     valid;
};

struct SessionContext {
    uint64_t        session_id;
    bool            active;
    bool            authenticated;
    uint32_t        permissions;
    uint64_t        created_at;
    uint64_t        last_access;
    RotationSession rotation;
    LRUCache        cache;
};

struct SessionManager {
    SessionContext sessions[SESSION_MAX_ACTIVE];
    int            session_count;
    uint64_t       next_session_id;
    uint64_t       clock;
    bool           initialized;
    IteratorHandle iterators[SESSION_MAX_ITERATORS];
    int            iterator_count;
    uint64_t       next_iterator_id;
};

void session_manager_init(SessionManager *sm);
void session_manager_destroy(SessionManager *sm);
int  session_create(SessionManager *sm, SessionContext **out);
int  session_destroy_ctx(SessionManager *sm, uint64_t session_id);
SessionContext *session_find(SessionManager *sm, uint64_t session_id);
int  session_authenticate(SessionContext *ctx, const uint8_t *credentials, size_t len);
int  session_refresh(SessionContext *ctx);
bool session_is_expired(const SessionContext *ctx, uint64_t current_time);
int  session_register_iterator(SessionManager *sm, uint64_t session_id,
                               void *node, uint64_t *iter_id_out);
int  session_unregister_iterator(SessionManager *sm, uint64_t iter_id);
void session_invalidate_iterators_for_node(SessionManager *sm, void *node);
int  session_count_active(const SessionManager *sm);
void session_cleanup_expired(SessionManager *sm, uint64_t current_time);
int  session_set_permissions(SessionContext *ctx, uint32_t perms);
uint32_t session_get_permissions(const SessionContext *ctx);

#endif
