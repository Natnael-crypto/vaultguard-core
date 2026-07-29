#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "engine/ranged.h"
#include "engine/item.h"
#include "engine/entity.h"
#include "engine/map.h"
#include "core/world.h"
#include "systems/rng.h"

#define TEST(name) printf("  %-40s", #name);
#define PASS()     printf("PASS\n"); pass_count++;
#define FAIL(msg)  printf("FAIL: %s\n", msg); fail_count++;

static int pass_count = 0;
static int fail_count = 0;

static void test_ranged_ammo_init(void) {
    AmmoDef defs[MAX_AMMO_TYPES];
    int count = 0;
    ranged_init_ammo_defs(defs, &count);

    TEST("ranged_init_ammo_defs creates ammo types");
    if (count > 0 && defs[0].type == AM_ARROW) PASS();
    else FAIL("no ammo types created");

    TEST("ranged defs have proper damage values");
    if (defs[0].damage_dice >= 1 && defs[0].damage_sides >= 1) PASS();
    else FAIL("bad damage values");

    int idx = ranged_get_ammo_index(AM_ARROW, defs, count);
    TEST("ranged_get_ammo_index finds arrow");
    if (idx >= 0 && defs[idx].type == AM_ARROW) PASS();
    else FAIL("arrow not found");

    idx = ranged_get_ammo_index(99, defs, count);
    TEST("ranged_get_ammo_index returns -1 for invalid");
    if (idx < 0) PASS();
    else FAIL("invalid type returned index");
}

static void test_ranged_weapon_create(void) {
    RangedWeapon w;
    ranged_create_weapon(&w, "longbow", ')', 1, 8, 2, 10, AM_ARROW);

    TEST("ranged_create_weapon sets name");
    if (strcmp(w.name, "longbow") == 0) PASS();
    else FAIL("wrong name");

    TEST("ranged_create_weapon sets damage");
    if (w.damage_dice == 1 && w.damage_sides == 8 && w.damage_bonus == 2) PASS();
    else FAIL("wrong damage");

    TEST("ranged_create_weapon sets range");
    if (w.range == 10) PASS();
    else FAIL("wrong range");

    TEST("ranged_create_weapon sets ammo type");
    if (w.ammo_type == AM_ARROW) PASS();
    else FAIL("wrong ammo type");
}

static void test_ranged_line_of_sight(void) {
    World w;
    memset(&w, 0, sizeof(World));
    w.map = (Map *)malloc(sizeof(Map));
    map_init(w.map, 40, 20);
    for (int x = 5; x < 35; x++)
        for (int y = 5; y < 15; y++)
            w.map->tiles[x][y] = TILE_FLOOR;

    TEST("ranged_target_in_los clear path");
    if (ranged_target_in_line_of_sight(&w, 10, 10, 20, 10)) PASS();
    else FAIL("should have los");

    w.map->tiles[15][10] = TILE_WALL;
    TEST("ranged_target_in_los blocked by wall");
    if (!ranged_target_in_line_of_sight(&w, 10, 10, 25, 10)) PASS();
    else FAIL("should be blocked");

    free(w.map);
}

static void test_ranged_hit_chance(void) {
    Entity *shooter = entity_create("shooter", 'h', 10, 10);
    Entity *target = entity_create("target", 't', 15, 10);
    RangedWeapon w;
    ranged_create_weapon(&w, "shortbow", ')', 1, 6, 1, 8, AM_ARROW);

    int chance = ranged_calculate_hit_chance(shooter, target, &w, 8);
    TEST("ranged_calculate_hit_chance returns positive");
    if (chance > 0 && chance <= 100) PASS();
    else FAIL("bad hit chance");

    target->x = 100;
    chance = ranged_calculate_hit_chance(shooter, target, &w, 8);
    TEST("ranged_calculate_hit_chance returns 0 out of range");
    if (chance == 0) PASS();
    else FAIL("should be 0 for out of range");

    entity_destroy(shooter); free(shooter);
    entity_destroy(target); free(target);
}

static void test_ranged_weapon_detection(void) {
    TEST("ranged_is_ranged_weapon returns false for null");
    if (!ranged_is_ranged_weapon(NULL)) PASS();
    else FAIL("null should not be ranged");

    Item *bow = item_create("shortbow", ')', ITEM_WEAPON);
    Item *sword = item_create("iron sword", '/', ITEM_WEAPON);

    TEST("ranged_is_ranged_weapon detects bow");
    if (ranged_is_ranged_weapon(bow)) PASS();
    else FAIL("bow not detected");

    TEST("ranged_is_ranged_weapon rejects sword");
    if (!ranged_is_ranged_weapon(sword)) PASS();
    else FAIL("sword detected as ranged");

    item_free(bow);
    item_free(sword);
}

static void test_ranged_fire(void) {
    RNGState rng;
    rng_init(&rng, 42);

    World w;
    memset(&w, 0, sizeof(World));
    w.map = (Map *)malloc(sizeof(Map));
    map_init(w.map, 40, 20);
    for (int x = 5; x < 35; x++)
        for (int y = 5; y < 15; y++)
            w.map->tiles[x][y] = TILE_FLOOR;

    Entity *shooter = entity_create("shooter", 'h', 10, 10);
    shooter->dexterity = 15;
    Entity *target = entity_create("target", 't', 15, 10);
    target->health = 30;
    target->max_health = 30;

    RangedWeapon rwp;
    ranged_create_weapon(&rwp, "shortbow", ')', 1, 6, 1, 8, AM_ARROW);

    AmmoDef defs[MAX_AMMO_TYPES];
    int count = 0;
    ranged_init_ammo_defs(defs, &count);

    int dmg = ranged_fire_weapon(shooter, target, &rwp, &defs[0], &w, &rng);
    TEST("ranged_fire_weapon returns damage");
    if (dmg > 0) PASS();
    else FAIL("no damage");

    entity_destroy(shooter); free(shooter);
    entity_destroy(target); free(target);
    free(w.map);
}

int main(void) {
    printf("=== Ranged tests ===\n");
    test_ranged_ammo_init();
    test_ranged_weapon_create();
    test_ranged_line_of_sight();
    test_ranged_hit_chance();
    test_ranged_weapon_detection();
    test_ranged_fire();
    printf("\n%d passed, %d failed\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
