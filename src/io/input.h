#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    KEY_NONE = 0,
    KEY_UP = 'w',
    KEY_DOWN = 's',
    KEY_LEFT = 'a',
    KEY_RIGHT = 'd',
    KEY_NW = 'q',
    KEY_NE = 'e',
    KEY_SW = 'z',
    KEY_SE = 'x',
    KEY_WAIT = '.',
    KEY_GET = 'g',
    KEY_DROP = 'f',
    KEY_USE = 'u',
    KEY_INVENTORY = 'i',
    KEY_EQUIPMENT = 'e',
    KEY_CHARACTER = 'c',
    KEY_HELP = '?',
    KEY_QUIT = 'Q',
    KEY_SAVE = 'S',
    KEY_LOAD = 'L',
    KEY_DEBUG = '!',
    KEY_MESSAGE_LOG = 'm',
    KEY_MINIMAP = 'M',
    KEY_STAIRS_UP = '<',
    KEY_STAIRS_DOWN = '>',
    KEY_EXAMINE = '/',
    KEY_REST = 'r',
    KEY_LOOK = 'l',
    KEY_PICK_TILE = 't',
    KEY_AUTO_EXPLORE = 'o',
    KEY_FIRE = 'f',
    KEY_OPEN = 'O',
    KEY_CLOSE = 'C',
    KEY_ENTER = '\n',
    KEY_ESCAPE = 27,
    KEY_BACKSPACE = 127,
    KEY_TAB = '\t',
    KEY_SPACE = ' '
} KeyCode;

void input_set_raw(int enable);
int input_get_key(void);
int input_get_key_timeout(int ms);
void input_flush(void);
bool input_key_pressed(int key);
const char *input_key_name(int key);

#endif
