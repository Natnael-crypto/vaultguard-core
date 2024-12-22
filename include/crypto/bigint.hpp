#ifndef VAULTGUARD_BIGINT_HPP
#define VAULTGUARD_BIGINT_HPP

#include <cstddef>
#include <cstdint>

#define BIGINT_MAX_LIMBS      128
#define BIGINT_DENSE_LIMBS    64
#define BIGINT_SPARSE_LIMBS   32
#define FMT_DENSE_32          0x01
#define FMT_SPARSE_64         0x02
#define FMT_OPTIMIZED         0x04
#define FMT_NEGATIVE          0x08
#define FMT_NORMALIZED        0x10

struct BigInt {
    uint32_t limbs32[BIGINT_DENSE_LIMBS];
    int      limb_count;
    uint32_t flags;
    size_t   allocated_size;
};

void    bigint_init(BigInt *bi);
void    bigint_free(BigInt *bi);
void    bigint_copy(BigInt *dst, const BigInt *src);
int     bigint_from_uint64(BigInt *bi, uint64_t val);
uint64_t bigint_to_uint64(const BigInt *bi);

#endif
