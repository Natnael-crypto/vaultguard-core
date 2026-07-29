#ifndef CRAFTING_H
#define CRAFTING_H

#include <stdint.h>
#include <stdbool.h>
#include "core/config.h"
#include "engine/item.h"
#include "engine/entity.h"
#include "systems/rng.h"

#define MAX_RECIPES 48
#define MAX_RECIPE_NAME 48
#define MAX_MATERIALS 8
#define MAX_CRAFT_STATION_NAME 32

typedef enum {
    CS_ANVIL,
    CS_CAULDRON,
    CS_ALTAR,
    CS_WORKBENCH,
    CS_ENCHANTING_TABLE,
    CS_FORGE,
    CS_ALCHEMY_BENCH
} CraftStation;

typedef struct {
    ItemType type;
    uint32_t item_id;
    int quantity;
    char name[32];
} CraftMaterial;

typedef struct {
    uint32_t id;
    char name[MAX_RECIPE_NAME];
    CraftStation station;
    int skill_required;
    int materials_count;
    CraftMaterial materials[MAX_MATERIALS];
    Item result;
    int result_quantity;
    char description[256];
    bool discovered;
} CraftRecipe;

typedef struct {
    int skill_level;
    int recipes_discovered;
    int items_crafted;
    char station_name[MAX_CRAFT_STATION_NAME];
} CraftingState;

int crafting_init_recipes(CraftRecipe *recipes, int *count);
int crafting_can_craft(const CraftRecipe *recipe, const Entity *crafter);
int crafting_execute(CraftRecipe *recipe, Entity *crafter, RNGState *rng);
int crafting_get_recipe_by_id(const CraftRecipe *recipes, int count, uint32_t id);
bool crafting_has_materials(const CraftRecipe *recipe, const Entity *crafter);
int crafting_discover_recipe(CraftRecipe *recipes, int count, uint32_t id);
const char *crafting_station_name(CraftStation cs);
int crafting_calculate_success(const CraftRecipe *recipe, int skill, RNGState *rng);
int crafting_generate_random_recipe(CraftRecipe *recipes, int count, int level, RNGState *rng);
void crafting_print_recipe(const CraftRecipe *recipe);
int crafting_get_available_recipes(const CraftRecipe *recipes, int count, CraftStation station, uint32_t *out, int max_out);
int crafting_get_recipes_by_tier(const CraftRecipe *recipes, int count, int min_tier, uint32_t *out, int max_out);
int crafting_get_recipes_by_type(const CraftRecipe *recipes, int count, ItemType type, uint32_t *out, int max_out);
int crafting_get_recipe_count_for_station(const CraftRecipe *recipes, int count, CraftStation station);

#endif
