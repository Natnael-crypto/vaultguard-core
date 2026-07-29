#ifndef RNG_H
#define RNG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint64_t state[4];
    uint64_t seed;
    uint64_t call_count;
} RNGState;

void rng_init(RNGState *rng, uint64_t seed);
void rng_seed(RNGState *rng, uint64_t seed);
uint64_t rng_next(RNGState *rng);
uint32_t rng_next_u32(RNGState *rng);
int rng_range(RNGState *rng, int min, int max);
float rng_float(RNGState *rng);
float rng_range_float(RNGState *rng, float min, float max);
int rng_dice(RNGState *rng, int sides);
int rng_roll(RNGState *rng, int count, int sides, int bonus);
bool rng_chance(RNGState *rng, int percent);
int rng_weighted(RNGState *rng, const int *weights, int count);
void rng_shuffle(RNGState *rng, void *array, int count, size_t size);
uint64_t rng_get_state(const RNGState *rng);

#endif
