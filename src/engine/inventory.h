#ifndef INVENTORY_H
#define INVENTORY_H

#include "engine/item.h"
#include "engine/entity.h"
#include <stdbool.h>

int inventory_add(Entity *owner, Item *it);
int inventory_remove(Entity *owner, int slot);
int inventory_find(const Entity *owner, uint32_t item_id);
int inventory_count(const Entity *owner);
Item *inventory_get(const Entity *owner, int slot);
bool inventory_has_space(const Entity *owner, int count);
int inventory_equip(Entity *owner, int slot);
int inventory_unequip(Entity *owner, int equip_slot);
int inventory_use(Entity *owner, int slot);
void inventory_drop_all(Entity *owner);
int inventory_get_equipped_count(const Entity *owner, EquipmentSlot slot);

#endif
