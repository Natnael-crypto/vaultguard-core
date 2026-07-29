#ifndef ENTITY_H
#define ENTITY_H

#include <stdint.h>
#include <stdbool.h>
#include "core/config.h"
#include "engine/map.h"
#include "systems/rng.h"

typedef struct Entity {
    uint32_t id;
    char name[MAX_NAME_LEN];
    char glyph;
    int x, y;
    int prev_x, prev_y;
    int health, max_health;
    int mana, max_mana;
    int strength, dexterity, intelligence;
    int attack, defense, armor;
    int speed, luck;
    int level, experience;
    int gold;
    bool alive;
    bool is_player;
    bool is_monster;
    bool is_item;
    bool blocks_movement;
    bool blocks_sight;
    AIBehavior behavior;
    int alert_range;
    int move_counter;
    int stun_turns;
    int poison_turns;
    int burn_turns;
    int slow_turns;
    int haste_turns;
    uint32_t flags;
    struct Entity *target;
    int inventory_capacity;
    int inventory_count;
    void **inventory;
    int equipment[SLOT_COUNT];
    int damage_dice;
    int damage_sides;
    int damage_bonus;
    int xp_value;
    int next_level_exp;
    int corpse_item_id;
    int faction_id;
    int vision_range;
    int path_step;
    int path_len;
    int path[MAX_MAP_WIDTH * MAX_MAP_HEIGHT];
    uint32_t ai_state_flags;
    void *ai_data;
} Entity;

typedef struct EntityNode {
    Entity *entity;
    struct EntityNode *next;
} EntityNode;

Entity *entity_create(const char *name, char glyph, int x, int y);
void entity_destroy(Entity *e);
void entity_take_damage(Entity *e, int amount, DamageType dtype);
void entity_heal(Entity *e, int amount);
void entity_apply_status(Entity *e, int poison, int burn, int slow, int haste);
void entity_tick_status(Entity *e);
bool entity_is_alive(const Entity *e);
int entity_get_stat(const Entity *e, StatModType stat);
int entity_roll_attack(const Entity *e);
int entity_roll_damage(const Entity *e);
bool entity_can_see(const Entity *e, int tx, int ty, const Map *map);

/* Monster template system */
int entity_get_monster_template_count(void);
const char *entity_get_monster_name(int template_idx);
Entity *entity_create_monster(const char *name, char glyph, int x, int y,
                              int depth, int template_idx);
Entity *entity_create_monster_for_depth(int depth, int x, int y, RNGState *rng);
Entity *entity_create_theme_monster(int depth, int x, int y, int theme,
                                     RNGState *rng);
int entity_get_xp_value(const Entity *e);
bool entity_has_status_effect(const Entity *e);
int entity_distance(const Entity *a, const Entity *b);
void entity_reset_path(Entity *e);
bool entity_has_path(const Entity *e);
int entity_next_path_step(Entity *e);

#endif
