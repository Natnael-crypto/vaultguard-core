#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "engine/ai.h"
#include "engine/entity.h"
#include "engine/combat.h"
#include "engine/map.h"
#include "engine/dungeon.h"
#include "core/world.h"

#define TEST(name) printf("  %-40s", #name);
#define PASS()     printf("PASS\n"); pass_count++;
#define FAIL(msg)  printf("FAIL: %s\n", msg); fail_count++;

static int pass_count = 0;
static int fail_count = 0;

static World *create_test_world(void) {
    World *w = (World *)calloc(1, sizeof(World));
    w->map = (Map *)calloc(1, sizeof(Map));
    map_init(w->map, 40, 20);

    for (int x = 10; x < 30; x++)
        for (int y = 5; y < 15; y++)
            w->map->tiles[x][y] = TILE_FLOOR;

    w->player = entity_create("player", '@', 15, 10);
    w->player->alive = true;
    w->player->is_player = true;

    return w;
}

static void destroy_test_world(World *w) {
    if (w->player) { entity_destroy(w->player); free(w->player); }
    for (int i = 0; i < w->monster_count; i++) {
        if (w->monsters[i]) { entity_destroy(w->monsters[i]); free(w->monsters[i]); }
    }
    if (w->map) free(w->map);
    free(w);
}

static void test_ai_wander(void) {
    World *w = create_test_world();
    Entity *mon = entity_create("test_monster", 't', 20, 10);
    mon->alive = true;
    mon->is_monster = true;
    mon->behavior = AI_WANDER;
    mon->move_counter = 0;

    int ox = mon->x, oy = mon->y;
    ai_update(mon, w);
    TEST("ai_wander moves monster");
    if (mon->x != ox || mon->y != oy) PASS();
    else FAIL("did not move");

    entity_destroy(mon);
    free(mon);
    destroy_test_world(w);
}

static void test_ai_aggressive(void) {
    World *w = create_test_world();
    Entity *mon = entity_create("aggressive_monster", 'A', 18, 10);
    mon->alive = true;
    mon->is_monster = true;
    mon->behavior = AI_AGGRESSIVE;
    mon->alert_range = 20;

    int ox = mon->x;
    ai_update(mon, w);
    TEST("ai_aggressive moves toward player");
    if (mon->x < ox) PASS();
    else FAIL("did not move toward player");

    entity_destroy(mon);
    free(mon);
    destroy_test_world(w);
}

static void test_ai_can_see(void) {
    World *w = create_test_world();
    Entity *mon = entity_create("mon", 'm', 20, 10);

    TEST("ai_can_see_player returns true when close");
    if (ai_can_see_player(mon, w)) PASS();
    else FAIL("cannot see player");

    mon->x = 100;
    mon->y = 100;
    TEST("ai_can_see_player returns false when far");
    if (!ai_can_see_player(mon, w)) PASS();
    else FAIL("can see player from far away");

    entity_destroy(mon);
    free(mon);
    destroy_test_world(w);
}

static void test_ai_melee_attack(void) {
    World *w = create_test_world();
    Entity *mon = entity_create("attacker", 'A', 14, 10);
    mon->alive = true;
    mon->is_monster = true;
    mon->attack = 10;
    mon->damage_dice = 1;
    mon->damage_sides = 6;
    mon->damage_bonus = 0;
    mon->strength = 10;

    int hp_before = w->player->health;
    ai_melee_attack(mon, w);
    TEST("ai_melee_attack damages player");
    if (w->player->health < hp_before) PASS();
    else FAIL("no damage dealt");

    entity_destroy(mon);
    free(mon);
    destroy_test_world(w);
}

int main(void) {
    printf("=== AI tests ===\n");
    test_ai_wander();
    test_ai_aggressive();
    test_ai_can_see();
    test_ai_melee_attack();
    printf("\n%d passed, %d failed\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
