#ifndef TRAP_H
#define TRAP_H

#include <stdint.h>
#include <stdbool.h>
#include "core/config.h"
#include "engine/entity.h"
#include "core/world.h"

#define MAX_TRAPS 64
#define MAX_TRAP_NAME 32

typedef enum {
    TRAP_ARROW,
    TRAP_POISON_DART,
    TRAP_PIT,
    TRAP_TELEPORT,
    TRAP_CONFUSION,
    TRAP_SLOW,
    TRAP_FIRE,
    TRAP_EXPLOSION,
    TRAP_ALARM,
    TRAP_SUMMON
} TrapType;

typedef struct {
    int x, y;
    TrapType type;
    bool triggered;
    bool visible;
    int damage;
    int detection_difficulty;
    char glyph;
    char name[MAX_TRAP_NAME];
} Trap;

typedef struct {
    Trap traps[MAX_TRAPS];
    int count;
} TrapList;

void trap_init(Trap *t, TrapType type, int x, int y);
void trap_list_init(TrapList *tl);
int trap_list_add(TrapList *tl, TrapType type, int x, int y);
bool trap_trigger(Trap *t, Entity *victim, World *w);
bool trap_detect(const Trap *t, int detect_power);
Trap *trap_at(TrapList *tl, int x, int y);
void trap_generate_for_level(TrapList *tl, Map *m, int depth, RNGState *rng);
int trap_place_random(TrapList *tl, const Map *m, int depth, RNGState *rng);
void trap_reveal_nearby(TrapList *tl, int px, int py, int radius);
const char *trap_name(TrapType type);
char trap_glyph(TrapType type);
int trap_damage(TrapType type, int depth);
int trap_detection_difficulty(TrapType type);

#endif
