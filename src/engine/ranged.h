#ifndef RANGED_H
#define RANGED_H

#include <stdint.h>
#include <stdbool.h>
#include "core/config.h"
#include "engine/entity.h"
#include "engine/item.h"
#include "core/world.h"
#include "systems/rng.h"

#define MAX_AMMO_TYPES 16
#define MAX_RANGED_WEAPONS 24
#define MAX_PROJECTILE_NAME 32

typedef enum {
    AM_NONE,
    AM_ARROW,
    AM_BOLT,
    AM_DART,
    AM_BULLET,
    AM_ROCK,
    AM_THROWING_KNIFE,
    AM_SPEAR,
    AM_JAVELIN,
    AM_MAGIC_DART
} AmmoType;

typedef struct {
    AmmoType type;
    char name[MAX_PROJECTILE_NAME];
    char glyph;
    int damage_dice;
    int damage_sides;
    int damage_bonus;
    int range;
    int accuracy_bonus;
    char description[128];
} AmmoDef;

typedef struct {
    uint32_t id;
    char name[MAX_NAME_LEN];
    char glyph;
    int damage_dice;
    int damage_sides;
    int damage_bonus;
    int range;
    int accuracy_bonus;
    AmmoType ammo_type;
    int shot_cost;      
    int two_handed;     
    char description[128];
} RangedWeapon;

int ranged_init_ammo_defs(AmmoDef *defs, int *count);
int ranged_get_ammo_index(AmmoType type, const AmmoDef *defs, int count);
int ranged_create_weapon(RangedWeapon *wpn, const char *name, char glyph, int dice, int sides, int bonus, int range, AmmoType ammo);
int ranged_fire_weapon(Entity *shooter, Entity *target, const RangedWeapon *wpn, const AmmoDef *ammo, World *w, RNGState *rng);
int ranged_get_ammo_count(const Entity *e, AmmoType type);
int ranged_consume_ammo(Entity *e, AmmoType type);
int ranged_calculate_hit_chance(const Entity *shooter, const Entity *target, const RangedWeapon *wpn, int range);
bool ranged_target_in_line_of_sight(const World *w, int x1, int y1, int x2, int y2);
int ranged_find_ammo_in_inventory(const Entity *e, AmmoType type);
bool ranged_is_ranged_weapon(const Item *it);
int ranged_get_weapon_range(const Item *it);

#endif
