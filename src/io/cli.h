#ifndef CLI_H
#define CLI_H

#include "core/game.h"

typedef enum {
    VS_NORMAL,
    VS_DEBUG,
    VS_QUIET
} VerboseSetting;

typedef struct {
    Game *game;
    int screen_width;
    int screen_height;
    bool running;
    bool paused;
    VerboseSetting verbose;
    int cursor_x;
    int cursor_y;
    char status_line[128];
} CliContext;

int cli_init(CliContext *ctx, Game *game);
void cli_destroy(CliContext *ctx);
void cli_run(CliContext *ctx);
void cli_render(CliContext *ctx);
void cli_handle_key(CliContext *ctx, int key);
void cli_draw_status(CliContext *ctx);
void cli_draw_messages(CliContext *ctx);
void cli_draw_minimap(CliContext *ctx);
void cli_show_inventory(CliContext *ctx);
void cli_show_equipment(CliContext *ctx);
void cli_show_character_sheet(CliContext *ctx);
void cli_show_help(CliContext *ctx);
void cli_show_message_log(CliContext *ctx);
void cli_show_debug(CliContext *ctx);

#endif
