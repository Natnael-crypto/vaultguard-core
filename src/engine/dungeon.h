#ifndef DUNGEON_H
#define DUNGEON_H

#include "engine/map.h"
#include "systems/rng.h"

typedef struct {
    int x, y, w, h;
    int cx, cy;
} Room;

typedef struct {
    Room rooms[MAX_DUNGEON_ROOMS];
    int room_count;
    int width;
    int height;
} Dungeon;

int dungeon_generate(Dungeon *d, int width, int height, int min_rooms, int max_rooms, RNGState *rng);
int dungeon_place_on_map(const Dungeon *d, Map *m, int depth);
int dungeon_connect_rooms(Dungeon *d, Map *m, RNGState *rng);
void dungeon_place_stairs(Dungeon *d, Map *m, bool down, RNGState *rng);
int dungeon_find_random_floor(const Map *m, int *x, int *y, RNGState *rng);
bool dungeon_room_overlaps(const Dungeon *d, int x, int y, int w, int h);
void dungeon_add_room(Dungeon *d, int x, int y, int w, int h);

const char *dungeon_get_theme_name(int depth);
int dungeon_get_theme_for_depth(int depth);
int dungeon_get_monster_density(int depth);
int dungeon_get_trap_chance(int depth);
void dungeon_add_decoration(Map *m, int theme, RNGState *rng);
void dungeon_add_vault_room(Dungeon *d, Map *m, RNGState *rng);

/* Biome-specific room generation */
void dungeon_carve_crypt_room(Dungeon *d, Map *m, const Room *r, RNGState *rng);
void dungeon_carve_forest_room(Dungeon *d, Map *m, const Room *r, RNGState *rng);
void dungeon_carve_lava_room(Dungeon *d, Map *m, const Room *r, RNGState *rng);
void dungeon_carve_ice_room(Dungeon *d, Map *m, const Room *r, RNGState *rng);
void dungeon_carve_sewer_room(Dungeon *d, Map *m, const Room *r, RNGState *rng);
void dungeon_carve_temple_room(Dungeon *d, Map *m, const Room *r, RNGState *rng);
void dungeon_carve_theme_room(Dungeon *d, Map *m, const Room *r, int theme, RNGState *rng);
void dungeon_carve_theme_corridor(Map *m, int x1, int y1, int x2, int y2, int theme, RNGState *rng);
void dungeon_apply_theme_placement(const Dungeon *d, Map *m, int theme, RNGState *rng);

#endif
