#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "engine/combat.h"
#include "engine/entity.h"

#define TEST(name) printf("  %-40s", #name);
#define PASS()     printf("PASS\n"); pass_count++;
#define FAIL(msg)  printf("FAIL: %s\n", msg); fail_count++;

static int pass_count = 0;
static int fail_count = 0;

static Entity *make_entity(int hp, int atk, int def, int armor) {
    Entity *e = entity_create("test", 't', 0, 0);
    if (!e) return NULL;
    e->health = hp;
    e->max_health = hp;
    e->attack = atk;
    e->defense = def;
    e->armor = armor;
    e->strength = 10;
    e->dexterity = 10;
    e->luck = 10;
    e->damage_dice = 1;
    e->damage_sides = 6;
    e->damage_bonus = 0;
    e->alive = true;
    return e;
}

static void test_combat_attack_hit(void) {
    Entity *atk = make_entity(100, 50, 0, 0);
    Entity *def = make_entity(50, 0, 0, 0);

    CombatResult cr;
    int r = combat_attack(atk, def, &cr);
    TEST("combat_attack hits with high attack");
    if (r > 0 && cr.hit) PASS();
    else FAIL("attack did not hit");

    entity_destroy(atk); free(atk);
    entity_destroy(def); free(def);
}

static void test_combat_attack_damage(void) {
    Entity *atk = make_entity(100, 20, 0, 0);
    Entity *def = make_entity(50, 0, 0, 0);

    CombatResult cr;
    combat_attack(atk, def, &cr);
    TEST("combat_attack deals damage");
    if (cr.total_damage > 0 && def->health < 50) PASS();
    else FAIL("no damage dealt");

    entity_destroy(atk); free(atk);
    entity_destroy(def); free(def);
}

static void test_combat_kill(void) {
    Entity *atk = make_entity(100, 99, 0, 0);
    atk->damage_dice = 10;
    atk->damage_sides = 10;
    atk->damage_bonus = 0;
    Entity *def = make_entity(1, 0, 0, 0);

    CombatResult cr;
    combat_attack(atk, def, &cr);
    TEST("combat_attack can kill");
    if (!def->alive) PASS();
    else FAIL("target still alive");

    entity_destroy(atk); free(atk);
    entity_destroy(def); free(def);
}

static void test_combat_ranged(void) {
    Entity *atk = make_entity(100, 20, 0, 0);
    Entity *def = make_entity(50, 0, 0, 0);

    CombatResult cr;
    int r = combat_ranged_attack(atk, def, 5, &cr);
    TEST("combat_ranged_attack works");
    if (r > 0 && cr.total_damage > 0) PASS();
    else FAIL("ranged attack failed");

    entity_destroy(atk); free(atk);
    entity_destroy(def); free(def);
}

static void test_combat_critical(void) {
    TEST("combat_check_critical can trigger");
    bool crit = false;
    for (int i = 0; i < 1000; i++) {
        if (combat_check_critical(100)) {
            crit = true;
            break;
        }
    }
    if (crit) PASS();
    else FAIL("never critical");
}

int main(void) {
    printf("=== Combat tests ===\n");
    test_combat_attack_hit();
    test_combat_attack_damage();
    test_combat_kill();
    test_combat_ranged();
    test_combat_critical();
    printf("\n%d passed, %d failed\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
