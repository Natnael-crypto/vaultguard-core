#ifndef EQUIPMENT_H
#define EQUIPMENT_H

#include "core/config.h"
#include "engine/entity.h"
#include "engine/item.h"
#include "systems/rng.h"

/* Equipment slot display names */
const char *equipment_slot_name(int slot);

/* Create equipment items by type */
int equipment_create_ring(Item *it, int depth, RNGState *rng);
int equipment_create_amulet(Item *it, int depth, RNGState *rng);
int equipment_create_boots(Item *it, int depth, RNGState *rng);
int equipment_create_gloves(Item *it, int depth, RNGState *rng);
int equipment_create_helmet(Item *it, int depth, RNGState *rng);
int equipment_create_shield(Item *it, int depth, RNGState *rng);

/* Generate random equipment for loot drops */
int equipment_generate_random(Item *it, int depth, RNGState *rng);

/* Equipment quality names */
const char *equipment_quality_name(int quality);

/* Apply equipment stats to entity */
void equipment_apply_stats(Entity *e, const Item *it, bool equipped);

/* Equipment rarity roll */
int equipment_roll_quality(RNGState *rng, int depth);

/* Item type classification helpers */
bool equipment_is_ring(const Item *it);
bool equipment_is_amulet(const Item *it);
bool equipment_is_boots(const Item *it);
bool equipment_is_gloves(const Item *it);
bool equipment_is_helmet(const Item *it);
bool equipment_is_shield(const Item *it);

/* Ring affix types */
typedef enum {
    RAF_NONE,
    RAF_STRENGTH,
    RAF_DEXTERITY,
    RAF_INTELLIGENCE,
    RAF_ATTACK,
    RAF_DEFENSE,
    RAF_ARMOR,
    RAF_LUCK,
    RAF_HEALTH,
    RAF_MANA,
    RAF_REGENERATION
} RingAffix;

const char *ring_affix_name(RingAffix affix);

/* Generate description for equipment */
void equipment_build_description(Item *it);

/* Item set system */
int equipment_get_set_count(void);
const char *equipment_get_set_name(int set_idx);
int equipment_get_set_pieces(int set_idx);
int equipment_count_set_pieces_equipped(int set_idx, const Entity *e);
int equipment_get_set_bonus(int set_idx, int pieces_equipped);
int equipment_get_set_bonus_type(int set_idx);
int equipment_find_set_for_item(const Item *it);

#endif
