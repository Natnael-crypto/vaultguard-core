#ifndef AI_H
#define AI_H

#include "engine/entity.h"
#include "core/world.h"
#include "core/config.h"

void ai_update(Entity *e, World *w);
void ai_wander(Entity *e, World *w);
void ai_patrol(Entity *e, World *w);
void ai_aggressive(Entity *e, World *w);
void ai_coward(Entity *e, World *w);
void ai_guard(Entity *e, World *w);
bool ai_can_see_player(const Entity *e, const World *w);
bool ai_player_in_range(const Entity *e, const World *w, int range);
void ai_move_toward(Entity *e, World *w, int tx, int ty);
void ai_move_away(Entity *e, World *w, int tx, int ty);
bool ai_adjacent_to_player(const Entity *e, const World *w);
void ai_melee_attack(Entity *e, World *w);
void ai_use_ability(Entity *e, World *w);

void ai_berserk(Entity *e, World *w);
void ai_pack(Entity *e, World *w);
void ai_sniper(Entity *e, World *w);
void ai_summoner(Entity *e, World *w);
void ai_flee(Entity *e, World *w);

int ai_count_allies_nearby(const Entity *e, const World *w, int range);
int ai_count_enemies_nearby(const Entity *e, const World *w, int range);
bool ai_has_path_to(const Entity *e, const World *w, int tx, int ty);
void ai_move_to_safe_position(Entity *e, World *w, int from_x, int from_y);
Entity *ai_find_nearest_ally(Entity *e, const World *w, int range);
Entity *ai_find_weakest_enemy(Entity *e, const World *w, int range);

void ai_cache_target(Entity *target);
void ai_clear_cache(void);

#endif
