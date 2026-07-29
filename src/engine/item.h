#ifndef ITEM_H
#define ITEM_H

#include <stdint.h>
#include <stdbool.h>
#include "core/config.h"
#include "systems/rng.h"

typedef struct {
    uint32_t id;
    char name[MAX_NAME_LEN];
    char glyph;
    int x, y;
    ItemType type;
    int value;
    int weight;
    int stack_count;
    int max_stack;
    bool identified;
    bool cursed;
    bool equipped;

    int damage_dice;
    int damage_sides;
    int damage_bonus;

    int armor_value;
    int armor_slot;

    int heal_amount;
    int mana_restore;

    int stat_mods[MOD_LUCK + 1];

    char description[256];
    uint32_t flags;
} Item;

int item_init(Item *it, const char *name, char glyph, ItemType type);
Item *item_create(const char *name, char glyph, ItemType type);
Item *item_clone(const Item *src);
void item_destroy(Item *it);
void item_free(Item *it);
bool item_is_equippable(const Item *it);
bool item_is_usable(const Item *it);
bool item_is_stackable(const Item *it);
int item_get_equip_slot(const Item *it);
const char *item_type_name(ItemType type);

/* Item generation */
Item *item_generate_for_depth(int depth, RNGState *rng);

/* Item description */
void item_set_description(Item *it, const char *desc);

/* Item flags */
bool item_has_flag(const Item *it, uint32_t flag);
void item_set_flag(Item *it, uint32_t flag);
void item_clear_flag(Item *it, uint32_t flag);

/* Item quality */
const char *item_quality_label(int value);
int item_compare_quality(const Item *a, const Item *b);

#endif
