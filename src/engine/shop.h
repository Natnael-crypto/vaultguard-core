#ifndef SHOP_H
#define SHOP_H

#include <stdint.h>
#include <stdbool.h>
#include "core/config.h"
#include "engine/item.h"
#include "engine/entity.h"
#include "systems/rng.h"

#define MAX_SHOP_ITEMS 24
#define MAX_SHOP_NAME 48
#define MAX_MERCHANTS 8

typedef enum {
    MT_GENERAL,
    MT_WEAPONSMITH,
    MT_ARMORER,
    MT_POTION_SELLER,
    MT_SCROLL_SELLER,
    MT_MAGIC_SHOP
} MerchantType;

typedef struct {
    MerchantType type;
    char name[MAX_SHOP_NAME];
    int x, y;
    int level;
    int items_for_sale[MAX_SHOP_ITEMS];
    int item_count;
    float price_modifier;
    bool hostile;
    int faction_id;
} Merchant;

typedef struct {
    Merchant merchants[MAX_MERCHANTS];
    int count;
} MerchantList;

int shop_init_merchant(Merchant *m, MerchantType type, int x, int y, int level);
void shop_generate_inventory(Merchant *m, int level, RNGState *rng);
int shop_buy(Merchant *m, Entity *buyer, int item_index);
int shop_sell(Merchant *m, Entity *seller, int slot, Item *it);
int shop_get_price(const Merchant *m, int item_index, bool buying);
void shop_print_inventory(const Merchant *m);
const char *shop_merchant_name(MerchantType type);
int shop_find_merchant_at(MerchantList *ml, int x, int y);
void merchant_list_init(MerchantList *ml);
int merchant_list_add(MerchantList *ml, MerchantType type, int x, int y, int level);
int shop_trade(Entity *player, Merchant *m);

#endif
