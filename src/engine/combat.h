#ifndef COMBAT_H
#define COMBAT_H

#include "engine/entity.h"
#include "core/config.h"
#include "systems/rng.h"

typedef struct {
    int total_damage;
    int blocked;
    bool hit;
    bool critical;
    DamageType dtype;
    char description[128];
} CombatResult;

int combat_attack(Entity *attacker, Entity *defender, CombatResult *result);
int combat_ranged_attack(Entity *attacker, Entity *defender, int range, CombatResult *result);
int combat_calculate_damage(int attack, int defense, int dice, int sides, int bonus, RNGState *rng);
bool combat_check_hit(int attack, int defense, RNGState *rng);
bool combat_check_critical(int luck, RNGState *rng);
void combat_apply_damage(Entity *target, int amount, DamageType dtype);
int combat_get_defense(const Entity *e);
int combat_get_attack_rating(const Entity *e);

void combat_set_rng(RNGState *rng);

#endif
