#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "engine/crafting.h"
#include "engine/item.h"
#include "engine/entity.h"
#include "engine/inventory.h"
#include "systems/rng.h"

#define TEST(name) printf("  %-40s", #name);
#define PASS()     printf("PASS\n"); pass_count++;
#define FAIL(msg)  printf("FAIL: %s\n", msg); fail_count++;

static int pass_count = 0;
static int fail_count = 0;

static void test_crafting_recipes_init(void) {
    CraftRecipe recipes[MAX_RECIPES];
    int count = 0;
    crafting_init_recipes(recipes, &count);

    TEST("crafting_init_recipes creates recipes");
    if (count > 0) PASS();
    else FAIL("no recipes created");

    TEST("recipes have proper names");
    if (recipes[0].name[0] != '\0') PASS();
    else FAIL("empty recipe name");

    TEST("recipes have stations assigned");
    if (recipes[0].station >= CS_ANVIL && recipes[0].station <= CS_ALCHEMY_BENCH)
        PASS();
    else FAIL("invalid station");

    int idx = crafting_get_recipe_by_id(recipes, count, 1);
    TEST("crafting_get_recipe_by_id finds recipe");
    if (idx >= 0) PASS();
    else FAIL("recipe 1 not found");

    idx = crafting_get_recipe_by_id(recipes, count, 999);
    TEST("crafting_get_recipe_by_id returns -1 for invalid");
    if (idx < 0) PASS();
    else FAIL("invalid recipe found");
}

static void test_crafting_station_names(void) {
    TEST("crafting_station_name returns non-empty");
    if (crafting_station_name(CS_ANVIL)[0] != '\0' &&
        crafting_station_name(CS_FORGE)[0] != '\0') PASS();
    else FAIL("empty station name");
}

static void test_crafting_materials_check(void) {
    CraftRecipe recipes[MAX_RECIPES];
    int count = 0;
    crafting_init_recipes(recipes, &count);

    Entity *crafter = entity_create("crafter", 'c', 0, 0);
    crafter->inventory_capacity = 32;
    crafter->inventory_count = 0;
    crafter->inventory = (void **)malloc(sizeof(void *) * 32);

    TEST("crafting_has_materials returns false with empty inventory");
    if (!crafting_has_materials(&recipes[0], crafter)) PASS();
    else FAIL("should not have materials");

    Item *wood = item_create("wood", '/', ITEM_WEAPON);
    wood->stack_count = 5;
    inventory_add(crafter, wood);

    Item *wood2 = item_create("wood", '/', ITEM_WEAPON);
    wood2->stack_count = 5;
    inventory_add(crafter, wood2);

    TEST("crafting_has_materials returns true when enough");
    if (crafting_has_materials(&recipes[0], crafter)) PASS();
    else FAIL("should have materials");

    TEST("crafting_can_craft returns 0 when possible");
    if (crafting_can_craft(&recipes[0], crafter) == 0) PASS();
    else FAIL("can_craft failed");

    entity_destroy(crafter);
    free(crafter);
}

static void test_crafting_execute(void) {
    CraftRecipe recipes[MAX_RECIPES];
    int count = 0;
    crafting_init_recipes(recipes, &count);

    RNGState rng;
    rng_init(&rng, 42);

    Entity *crafter = entity_create("crafter", 'c', 0, 0);
    crafter->inventory_capacity = 32;
    crafter->inventory_count = 0;
    crafter->inventory = (void **)malloc(sizeof(void *) * 32);

    Item *mat = item_create("wood", '/', ITEM_WEAPON);
    mat->stack_count = 5;
    inventory_add(crafter, mat);

    int before = inventory_count(crafter);
    int rc = crafting_execute(&recipes[0], crafter, &rng);
    TEST("crafting_execute returns 0 or -4");
    if (rc == 0 || rc == -4) PASS();
    else FAIL("execute failed with wrong code");

    entity_destroy(crafter);
    free(crafter);
}

static void test_crafting_get_available(void) {
    CraftRecipe recipes[MAX_RECIPES];
    int count = 0;
    crafting_init_recipes(recipes, &count);

    uint32_t out[MAX_RECIPES];
    int found = crafting_get_available_recipes(recipes, count, CS_FORGE, out, MAX_RECIPES);
    TEST("crafting_get_available_recipes finds forge recipes");
    if (found > 0) PASS();
    else FAIL("no forge recipes found");

    found = crafting_get_available_recipes(recipes, count, CS_ANVIL, out, MAX_RECIPES);
    TEST("crafting_get_available_recipes for anvil");
    if (found >= 0) PASS();
    else FAIL("anvil lookup failed");
}

int main(void) {
    printf("=== Crafting tests ===\n");
    test_crafting_recipes_init();
    test_crafting_station_names();
    test_crafting_materials_check();
    test_crafting_execute();
    test_crafting_get_available();
    printf("\n%d passed, %d failed\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
