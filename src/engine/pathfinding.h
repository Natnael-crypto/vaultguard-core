#ifndef PATHFINDING_H
#define PATHFINDING_H

#include "engine/map.h"
#include <stdbool.h>

#define PF_MAX_NODES 4096
#define PF_MAX_PATH 512

typedef struct {
    int x, y;
    int g, h, f;
    int parent;
    int generation;
    bool closed;
    bool open;
} PFNode;

typedef struct {
    PFNode nodes[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
    int open_list[PF_MAX_NODES];
    int open_count;
    int path[PF_MAX_PATH][2];
    int path_len;
    int generation;
} Pathfinder;

void pf_init(Pathfinder *pf);
int pf_find_path(Pathfinder *pf, const Map *map, int sx, int sy, int ex, int ey,
                 bool (*is_walkable)(const Map *, int, int));
int pf_get_path(const Pathfinder *pf, int step, int *x, int *y);
bool pf_path_exists(const Pathfinder *pf);
int pf_path_length(const Pathfinder *pf);
void pf_clear(Pathfinder *pf);
Pathfinder *pf_get_global(void);

#endif
