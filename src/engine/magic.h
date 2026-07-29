#ifndef MAGIC_H
#define MAGIC_H

#include <stdint.h>
#include <stdbool.h>
#include "core/config.h"
#include "engine/entity.h"
#include "core/world.h"
#include "systems/rng.h"

#define MAX_SPELLS 32
#define MAX_SPELL_NAME 32
#define SPELL_TARGET_NONE 0
#define SPELL_TARGET_SELF 1
#define SPELL_TARGET_ENEMY 2
#define SPELL_TARGET_AREA 3

typedef enum {
    SPT_FIREBALL,
    SPT_HEAL,
    SPT_LIGHTNING,
    SPT_TELEPORT,
    SPT_CONFUSE,
    SPT_SLOW,
    SPT_HASTE,
    SPT_SHIELD,
    SPT_MAGIC_MISSILE,
    SPT_BLINK,
    SPT_DETECT,
    SPT_FEAR,
    SPT_POISON_CLOUD,
    SPT_BERSERK,
    SPT_REVEAL,
    SPT_ICE_SHARD,
    SPT_CHAIN_LIGHTNING,
    SPT_INVISIBILITY,
    SPT_STONE_SKIN,
    SPT_SUMMON_ELEMENTAL,
    SPT_DRAIN_LIFE,
    SPT_LIGHT,
    SPT_DISPEL_MAGIC
} SpellType;

typedef enum {
    SC_OFFENSE,
    SC_DEFENSE,
    SC_HEALING,
    SC_UTILITY,
    SC_DEBUFF
} SpellSchool;

typedef struct {
    SpellType type;
    char name[MAX_SPELL_NAME];
    int mana_cost;
    int min_level;
    int target_type;
    int damage;
    int damage_dice;
    int damage_sides;
    int range;
    int radius;
    int effect_turns;
    int effect_power;
    char description[128];
} Spell;

void magic_init_spells(Spell *spells, int *count);
int magic_cast_spell(Entity *caster, Spell *spell, World *w, RNGState *rng);
int magic_find_spell(const Spell *spells, int count, SpellType type);
int magic_learn_spell(Entity *e, SpellType type);
bool magic_has_spell(const Entity *e, SpellType type);
void magic_apply_effect(Entity *target, SpellType type, int power, int turns);
bool magic_target_in_range(const Entity *caster, int tx, int ty, int range);
void magic_area_effect(World *w, int cx, int cy, int radius, int damage, SpellType type);
int magic_get_target_type(SpellType type);
const char *magic_spell_name(SpellType type);
const char *magic_school_name(SpellSchool school);
int magic_get_school_spells(SpellSchool school, SpellType *out, int max_out);
SpellSchool magic_get_spell_school(SpellType type);
void magic_init_spell_scrolls(Spell *scrolls, int *count);
int magic_generate_random_scroll(int depth, RNGState *rng);
int magic_count_spells_in_school(const Spell *spells, int total_count, SpellSchool school);
int magic_get_mana_cost(const Spell *spell, int caster_level);
bool magic_can_cast(const Entity *caster, const Spell *spell);

#endif
