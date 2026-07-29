#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_EVENTS 256
#define MAX_EVENT_DATA 128

typedef enum {
    EV_NONE,
    EV_MONSTER_DEATH,
    EV_ITEM_PICKUP,
    EV_ITEM_DROP,
    EV_LEVEL_CHANGE,
    EV_COMBAT_HIT,
    EV_COMBAT_KILL,
    EV_PLAYER_MOVE,
    EV_PLAYER_ATTACK,
    EV_PLAYER_LEVELUP,
    EV_DOOR_OPEN,
    EV_DOOR_CLOSE,
    EV_STAIRS_USE,
    EV_TRIGGER_TRAP,
    EV_USE_ITEM,
    EV_EQUIP_ITEM,
    EV_UNEQUIP_ITEM,
    EV_STATUS_CHANGE,
    EV_GAME_OVER,
    EV_GAME_WIN,
    EV_MESSAGE,
    EV_TICK,
    EV_FOV_UPDATE,
    EV_SAVE_GAME,
    EV_LOAD_GAME
} EventType;

typedef struct {
    EventType type;
    uint32_t source_id;
    uint32_t target_id;
    int x, y;
    int value1;
    int value2;
    uint32_t timestamp;
    char data[MAX_EVENT_DATA];
    bool handled;
    void *event_data;
    bool own_data;
} GameEvent;

typedef struct {
    GameEvent events[MAX_EVENTS];
    int head;
    int tail;
    int count;
    uint32_t next_id;
    uint32_t timestamp;
} EventQueue;

void event_queue_init(EventQueue *eq);
int event_queue_push(EventQueue *eq, const GameEvent *evt);
int event_queue_push_heap(EventQueue *eq, const GameEvent *evt, void *data);
int event_queue_pop(EventQueue *eq, GameEvent *evt);
int event_queue_peek(const EventQueue *eq, GameEvent *evt);
int event_queue_count(const EventQueue *eq);
void event_queue_clear(EventQueue *eq);
bool event_queue_has_type(const EventQueue *eq, EventType type);
int event_queue_remove_type(EventQueue *eq, EventType type);
GameEvent event_make(EventType type, uint32_t source, uint32_t target, int x, int y, int v1, int v2);
void event_free_data(GameEvent *evt);

#endif
