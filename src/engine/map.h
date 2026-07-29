#ifndef MAP_H
#define MAP_H

#include <stdint.h>
#include <stdbool.h>
#include "core/config.h"
#include "systems/rng.h"

typedef struct {
    int width;
    int height;
    char tiles[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
    bool visible[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
    bool explored[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
    int light[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
    uint32_t flags[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
    int room_count;
    struct {
        int x, y, w, h;
        int cx, cy;
    } rooms[MAX_DUNGEON_ROOMS];
    int stairs_down_x;
    int stairs_down_y;
    int stairs_up_x;
    int stairs_up_y;
} Map;

int map_init(Map *m, int width, int height);
void map_clear(Map *m, char fill);
bool map_in_bounds(const Map *m, int x, int y);
bool map_is_walkable(const Map *m, int x, int y);
bool map_is_transparent(const Map *m, int x, int y);
void map_set_tile(Map *m, int x, int y, char tile);
char map_get_tile(const Map *m, int x, int y);
int map_get_room_at(const Map *m, int x, int y);
void map_carve_room(Map *m, int x, int y, int w, int h);
void map_carve_corridor(Map *m, int x1, int y1, int x2, int y2);
void map_compute_fov(Map *m, int px, int py, int radius);
void map_set_visible(Map *m, int x, int y, bool v);
bool map_is_visible(const Map *m, int x, int y);
void map_copy(Map *dst, const Map *src);

/* Terrain and decoration helpers */
int map_count_tile_type(const Map *m, char tile);
int map_count_walkable_tiles(const Map *m);
void map_flood_fill(Map *m, int sx, int sy, char fill, char target);
void map_place_terrain_cluster(Map *m, char tile, int cx, int cy,
                                int radius, int density, RNGState *rng);
void map_carve_room_with_tiles(Map *m, int x, int y, int w, int h,
                                char floor_tile, char wall_tile);
void map_carve_river(Map *m, int sx, int sy, int ex, int ey, int width, RNGState *rng);
bool map_is_adjacent_to(const Map *m, int x, int y, char tile);
int map_find_nearest_tile(const Map *m, int x, int y, char tile, int max_dist,
                           int *ox, int *oy);
void map_add_tile_flags(Map *m, int x, int y, uint32_t flags);
void map_remove_tile_flags(Map *m, int x, int y, uint32_t flags);
bool map_has_tile_flags(const Map *m, int x, int y, uint32_t flags);
void map_set_light(Map *m, int x, int y, int intensity);
int map_get_light(const Map *m, int x, int y);

#endif
