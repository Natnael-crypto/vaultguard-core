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

struct SparseBigInt {
    uint64_t limbs64[BIGINT_SPARSE_LIMBS];
    int      limb_count;
    uint32_t flags;
    size_t   allocated_size;
    int      nonzero_indices[BIGINT_SPARSE_LIMBS];
    int      nonzero_count;
};

void    bigint_init(BigInt *bi);
void    bigint_free(BigInt *bi);
void    bigint_copy(BigInt *dst, const BigInt *src);
int     bigint_set_bytes(BigInt *bi, const uint8_t *data, size_t len);
int     bigint_to_bytes(const BigInt *bi, uint8_t *out, size_t out_len, size_t *written);
int     bigint_add(BigInt *result, const BigInt *a, const BigInt *b);
int     bigint_subtract(BigInt *result, const BigInt *a, const BigInt *b);
int     bigint_multiply(BigInt *result, const BigInt *a, const BigInt *b);
int     bigint_divide(BigInt *quotient, BigInt *remainder, const BigInt *a, const BigInt *b);
int     bigint_modpow(BigInt *result, const BigInt *base, const BigInt *exp, const BigInt *mod);
int     bigint_compare(const BigInt *a, const BigInt *b);
bool    bigint_is_zero(const BigInt *bi);
bool    bigint_is_one(const BigInt *bi);
void    bigint_set_zero(BigInt *bi);
void    bigint_set_one(BigInt *bi);
int     bigint_shift_left(BigInt *bi, int bits);
int     bigint_shift_right(BigInt *bi, int bits);
int     bigint_bitwise_and(BigInt *result, const BigInt *a, const BigInt *b);
int     bigint_bitwise_or(BigInt *result, const BigInt *a, const BigInt *b);
int     bigint_bitwise_xor(BigInt *result, const BigInt *a, const BigInt *b);
void    bigint_normalize(BigInt *bi);
int     bigint_bit_length(const BigInt *bi);
int     bigint_karatsuba(BigInt *result, const BigInt *a, const BigInt *b);
int     bigint_mul_optimized(BigInt *result, const BigInt *a, const BigInt *b);
void    sparse_bigint_init(SparseBigInt *sbi);
int     sparse_bigint_multiply(SparseBigInt *result, const SparseBigInt *a, const SparseBigInt *b);
int     bigint_to_sparse(SparseBigInt *dst, const BigInt *src);
int     sparse_to_bigint(BigInt *dst, const SparseBigInt *src);
uint32_t bigint_crc32(const BigInt *bi);
int     bigint_gcd(BigInt *result, const BigInt *a, const BigInt *b);
int     bigint_mod_inverse(BigInt *result, const BigInt *a, const BigInt *mod);
void    bigint_swap(BigInt *a, BigInt *b);
int     bigint_from_uint64(BigInt *bi, uint64_t val);
uint64_t bigint_to_uint64(const BigInt *bi);
int     bigint_random(BigInt *bi, int bit_length, uint32_t seed);

#endif
