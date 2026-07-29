#ifndef RENDERER_H
#define RENDERER_H

#include "engine/map.h"
#include "engine/entity.h"
#include "engine/item.h"
#include "core/config.h"

#define RENDER_BUF_W 160
#define RENDER_BUF_H 50

typedef struct {
    char chars[RENDER_BUF_H][RENDER_BUF_W];
    int colors[RENDER_BUF_H][RENDER_BUF_W];
    int width;
    int height;
    int offset_x;
    int offset_y;
} RenderBuffer;

void render_init(RenderBuffer *rb, int w, int h);
void render_clear(RenderBuffer *rb, char ch);
void render_map(RenderBuffer *rb, const Map *map, int px, int py, int radius);
void render_entities(RenderBuffer *rb, const Entity *player, const Entity *const *monsters, int mcount, const Map *map);
void render_items(RenderBuffer *rb, const Item *const *items, int icount, const Map *map);
void render_put_char(RenderBuffer *rb, int x, int y, char ch);
void render_put_string(RenderBuffer *rb, int x, int y, const char *str);
void render_draw_border(RenderBuffer *rb, int x, int y, int w, int h);
void render_fill_rect(RenderBuffer *rb, int x, int y, int w, int h, char ch);
void render_draw_bar(RenderBuffer *rb, int x, int y, int w, int val, int max, char fill, char empty);
void render_blit(RenderBuffer *rb, int dx, int dy, const RenderBuffer *src);
void render_scroll_to(RenderBuffer *rb, int target_x, int target_y, int view_w, int view_h);
void render_draw_minimap(RenderBuffer *rb, const Map *map, int px, int py, int w, int h);

#endif
