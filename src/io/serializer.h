#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "core/world.h"
#include "core/config.h"
#include "core/state.h"

int serializer_save_game(const World *w, const GameStats *stats, const char *path);
int serializer_load_game(World *w, GameStats *stats, const char *path);
int serializer_write_int32(FILE *f, int32_t val);
int serializer_write_uint32(FILE *f, uint32_t val);
int serializer_write_int16(FILE *f, int16_t val);
int serializer_write_uint16(FILE *f, uint16_t val);
int serializer_write_int8(FILE *f, int8_t val);
int serializer_write_uint8(FILE *f, uint8_t val);
int serializer_write_raw(FILE *f, const void *data, size_t len);
int serializer_write_string(FILE *f, const char *str);
int serializer_read_int32(FILE *f, int32_t *val);
int serializer_read_uint32(FILE *f, uint32_t *val);
int serializer_read_int16(FILE *f, int16_t *val);
int serializer_read_uint16(FILE *f, uint16_t *val);
int serializer_read_int8(FILE *f, int8_t *val);
int serializer_read_uint8(FILE *f, uint8_t *val);
int serializer_read_raw(FILE *f, void *data, size_t len);
int serializer_read_string(FILE *f, char *str, size_t max_len);

#endif
