#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "engine/map.h"

#define TEST(name) printf("  %-40s", #name);
#define PASS()     printf("PASS\n"); pass_count++;
#define FAIL(msg)  printf("FAIL: %s\n", msg); fail_count++;

static int pass_count = 0;
static int fail_count = 0;

static void test_map_init(void) {
    Map m;
    int r = map_init(&m, 60, 20);
    TEST("map_init creates map");
    if (r == 0 && m.width == 60 && m.height == 20) PASS();
    else FAIL("wrong dimensions");

    TEST("map_init fills with walls");
    if (m.tiles[0][0] == TILE_WALL) PASS();
    else FAIL("not wall");

    TEST("map_in_bounds returns true for valid");
    if (map_in_bounds(&m, 0, 0) && map_in_bounds(&m, 59, 19)) PASS();
    else FAIL("bounds check wrong");

    TEST("map_in_bounds returns false for invalid");
    if (!map_in_bounds(&m, -1, 0) && !map_in_bounds(&m, 60, 0)) PASS();
    else FAIL("bounds check wrong");
}

static void test_map_carve(void) {
    Map m;
    map_init(&m, 60, 20);
    map_carve_room(&m, 5, 5, 8, 6);

    TEST("carve_room creates floor tiles");
    if (m.tiles[5][5] == TILE_FLOOR && m.tiles[12][10] == TILE_FLOOR) PASS();
    else FAIL("room not carved");

    TEST("carve_room leaves walls outside");
    if (m.tiles[4][5] == TILE_WALL && m.tiles[5][4] == TILE_WALL) PASS();
    else FAIL("outside wall wrong");

    map_carve_corridor(&m, 5, 8, 20, 8);
    TEST("carve_corridor creates path");
    if (m.tiles[10][8] == TILE_FLOOR) PASS();
    else FAIL("corridor not carved");
}

static void test_map_walkable(void) {
    Map m;
    map_init(&m, 60, 20);

    TEST("wall is not walkable");
    if (!map_is_walkable(&m, 0, 0)) PASS();
    else FAIL("wall is walkable");

    map_set_tile(&m, 10, 10, TILE_FLOOR);
    TEST("floor is walkable");
    if (map_is_walkable(&m, 10, 10)) PASS();
    else FAIL("floor not walkable");

    map_set_tile(&m, 10, 11, TILE_DOOR);
    TEST("door is walkable");
    if (map_is_walkable(&m, 10, 11)) PASS();
    else FAIL("door not walkable");
}

static void test_map_transparent(void) {
    Map m;
    map_init(&m, 60, 20);

    TEST("wall is not transparent");
    if (!map_is_transparent(&m, 0, 0)) PASS();
    else FAIL("wall is transparent");

    map_set_tile(&m, 10, 10, TILE_FLOOR);
    TEST("floor is transparent");
    if (map_is_transparent(&m, 10, 10)) PASS();
    else FAIL("floor not transparent");
}

static void test_map_fov(void) {
    Map m;
    map_init(&m, 60, 20);
    map_carve_room(&m, 10, 5, 10, 10);

    map_compute_fov(&m, 15, 10, 8);

    TEST("fov makes player tile visible");
    if (map_is_visible(&m, 15, 10)) PASS();
    else FAIL("player not visible");

    TEST("fov marks explored tiles");
    if (m.explored[15][10]) PASS();
    else FAIL("not explored");
}

int main(void) {
    printf("=== Map tests ===\n");
    test_map_init();
    test_map_carve();
    test_map_walkable();
    test_map_transparent();
    test_map_fov();
    printf("\n%d passed, %d failed\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
