#include "crypto/bigint.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

void bigint_init(BigInt *bi) {
	memset(bi, 0, sizeof(BigInt));
	bi->flags = FMT_DENSE_32;
	bi->allocated_size = sizeof(BigInt);
}

void bigint_free(BigInt *bi) {
	if (!bi) return;
	memset(bi->limbs32, 0, sizeof(bi->limbs32));
	bi->limb_count = 0;
	bi->flags = 0;
}

void bigint_copy(BigInt *dst, const BigInt *src) {
	if (!dst || !src) return;
	memcpy(dst, src, sizeof(BigInt));
}

int bigint_set_bytes(BigInt *bi, const uint8_t *data, size_t len) {
	if (!bi || !data || len == 0) return -1;
	bigint_init(bi);

	int numLimbs = (int)((len + 3) / 4);
	if (numLimbs > BIGINT_DENSE_LIMBS) return -1;

	for (int i = 0; i < numLimbs; i++) {
		uint32_t val = 0;
		for (int j = 0; j < 4; j++) {
			int byte_idx = (int)len - 1 - (i * 4 + j);
			if (byte_idx >= 0) {
				val |= ((uint32_t)data[byte_idx]) << (j * 8);
			}
		}
		bi->limbs32[i] = val;
	}
	bi->limb_count = numLimbs;
	bigint_normalize(bi);
	return 0;
}

int bigint_to_bytes(const BigInt *bi, uint8_t *out, size_t out_len, size_t *written) {
	if (!bi || !out) return -1;

	int topLimb = bi->limb_count - 1;
	while (topLimb > 0 && bi->limbs32[topLimb] == 0) topLimb--;

	size_t needed = (size_t)(topLimb + 1) * 4;
	if (needed > out_len) return -1;

	size_t pos = 0;
	for (int i = topLimb; i >= 0; i--) {
		uint32_t v = bi->limbs32[i];
		out[pos++] = (uint8_t)(v >> 24);
		out[pos++] = (uint8_t)(v >> 16);
		out[pos++] = (uint8_t)(v >> 8);
		out[pos++] = (uint8_t)(v);
	}
	if (written) *written = pos;
	return 0;
}

int bigint_add(BigInt *result, const BigInt *a, const BigInt *b) {
	if (!result || !a || !b) return -1;

	int maxLen = a->limb_count > b->limb_count ? a->limb_count : b->limb_count;
	uint64_t carry = 0;

	for (int i = 0; i < maxLen; i++) {
		uint64_t sum = carry;
		if (i < a->limb_count) sum += a->limbs32[i];
		if (i < b->limb_count) sum += b->limbs32[i];
		result->limbs32[i] = (uint32_t)(sum & 0xFFFFFFFF);
		carry = sum >> 32;
	}

	if (carry && maxLen < BIGINT_DENSE_LIMBS) {
		result->limbs32[maxLen] = (uint32_t)carry;
		result->limb_count = maxLen + 1;
	} else {
		result->limb_count = maxLen;
	}

	result->flags = FMT_DENSE_32;
	bigint_normalize(result);
	return 0;
}

int bigint_subtract(BigInt *result, const BigInt *a, const BigInt *b) {
	if (!result || !a || !b) return -1;

	if (bigint_compare(a, b) < 0) {
		result->flags |= FMT_NEGATIVE;
		return bigint_subtract(result, b, a);
	}

	int64_t borrow = 0;
	int maxLen = a->limb_count;

	for (int i = 0; i < maxLen; i++) {
		int64_t diff = (int64_t)a->limbs32[i] - borrow;
		if (i < b->limb_count) diff -= b->limbs32[i];

		if (diff < 0) {
			diff += (int64_t)1 << 32;
			borrow = 1;
		} else {
			borrow = 0;
		}
		result->limbs32[i] = (uint32_t)(diff & 0xFFFFFFFF);
	}

	result->limb_count = maxLen;
	result->flags = FMT_DENSE_32;
	bigint_normalize(result);
	return 0;
}

int bigint_multiply(BigInt *result, const BigInt *a, const BigInt *b) {
	if (!result || !a || !b) return -1;

	if (a->limb_count + b->limb_count > BIGINT_DENSE_LIMBS) return -1;

	BigInt tmp;
	bigint_init(&tmp);

	for (int i = 0; i < a->limb_count; i++) {
		uint64_t carry = 0;
		for (int j = 0; j < b->limb_count; j++) {
			int k = i + j;
			if (k >= BIGINT_DENSE_LIMBS) break;
			uint64_t prod = (uint64_t)a->limbs32[i] * (uint64_t)b->limbs32[j];
			prod += tmp.limbs32[k];
			prod += carry;
			tmp.limbs32[k] = (uint32_t)(prod & 0xFFFFFFFF);
			carry = prod >> 32;
		}
		int k = i + b->limb_count;
		if (k < BIGINT_DENSE_LIMBS) {
			tmp.limbs32[k] += (uint32_t)carry;
		}
	}

	tmp.limb_count = a->limb_count + b->limb_count;
	if (tmp.limb_count > BIGINT_DENSE_LIMBS) tmp.limb_count = BIGINT_DENSE_LIMBS;
	tmp.flags = FMT_DENSE_32;
	bigint_normalize(&tmp);
	bigint_copy(result, &tmp);
	return 0;
}

int bigint_divide(BigInt *quotient, BigInt *remainder, const BigInt *a, const BigInt *b) {
	if (!quotient || !remainder || !a || !b) return -1;
	if (bigint_is_zero(b)) return -1;

	bigint_init(quotient);
	bigint_copy(remainder, a);

	if (bigint_compare(a, b) < 0) {
		bigint_set_zero(quotient);
		return 0;
	}

	int shift = bigint_bit_length(a) - bigint_bit_length(b);
	BigInt shifted;
	bigint_copy(&shifted, b);
	bigint_shift_left(&shifted, shift);

	for (int i = shift; i >= 0; i--) {
		if (bigint_compare(remainder, &shifted) >= 0) {
			bigint_subtract(remainder, remainder, &shifted);
			int limb_idx = i / 32;
			int bit_idx = i % 32;
			if (limb_idx < BIGINT_DENSE_LIMBS) {
				quotient->limbs32[limb_idx] |= (1U << bit_idx);
				if (limb_idx >= quotient->limb_count)
					quotient->limb_count = limb_idx + 1;
			}
		}
		bigint_shift_right(&shifted, 1);
	}

	bigint_normalize(quotient);
	bigint_normalize(remainder);
	return 0;
}

int bigint_modpow(BigInt *result, const BigInt *base, const BigInt *exp, const BigInt *mod) {
	if (!result || !base || !exp || !mod) return -1;
	if (bigint_is_zero(mod)) return -1;

	BigInt accum, b, q, r;
	bigint_init(&accum);
	bigint_set_one(&accum);
	bigint_copy(&b, base);

	bigint_divide(&q, &r, &b, mod);
	bigint_copy(&b, &r);

	int bits = bigint_bit_length(exp);
	for (int i = 0; i < bits; i++) {
		int limb_idx = i / 32;
		int bit_idx = i % 32;
		if (limb_idx < exp->limb_count && (exp->limbs32[limb_idx] & (1U << bit_idx))) {
			bigint_multiply(&accum, &accum, &b);
			bigint_divide(&q, &r, &accum, mod);
			bigint_copy(&accum, &r);
		}
		bigint_multiply(&b, &b, &b);
		bigint_divide(&q, &r, &b, mod);
		bigint_copy(&b, &r);
	}

	bigint_copy(result, &accum);
	return 0;
}

int bigint_compare(const BigInt *a, const BigInt *b) {
	if (!a || !b) return 0;

	int la = a->limb_count;
	int lb = b->limb_count;

	while (la > 0 && a->limbs32[la - 1] == 0) la--;
	while (lb > 0 && b->limbs32[lb - 1] == 0) lb--;

	if (la != lb) return la > lb ? 1 : -1;

	for (int i = la - 1; i >= 0; i--) {
		if (a->limbs32[i] != b->limbs32[i]) {
			return a->limbs32[i] > b->limbs32[i] ? 1 : -1;
		}
	}
	return 0;
}

bool bigint_is_zero(const BigInt *bi) {
	if (!bi) return true;
	for (int i = 0; i < bi->limb_count; i++) {
		if (bi->limbs32[i] != 0) return false;
	}
	return true;
}

bool bigint_is_one(const BigInt *bi) {
	if (!bi || bi->limb_count < 1) return false;
	if (bi->limbs32[0] != 1) return false;
	for (int i = 1; i < bi->limb_count; i++) {
		if (bi->limbs32[i] != 0) return false;
	}
	return true;
}

void bigint_set_zero(BigInt *bi) {
	if (!bi) return;
	memset(bi->limbs32, 0, sizeof(bi->limbs32));
	bi->limb_count = 1;
	bi->flags = FMT_DENSE_32;
}

void bigint_set_one(BigInt *bi) {
	if (!bi) return;
	memset(bi->limbs32, 0, sizeof(bi->limbs32));
	bi->limbs32[0] = 1;
	bi->limb_count = 1;
	bi->flags = FMT_DENSE_32;
}

int bigint_shift_left(BigInt *bi, int bits) {
	if (!bi || bits < 0) return -1;
	if (bits == 0) return 0;

	int word_shift = bits / 32;
	int bit_shift = bits % 32;

	if (word_shift > 0) {
		for (int i = BIGINT_DENSE_LIMBS - 1; i >= word_shift; i--) {
			bi->limbs32[i] = bi->limbs32[i - word_shift];
		}
		for (int i = 0; i < word_shift && i < BIGINT_DENSE_LIMBS; i++) {
			bi->limbs32[i] = 0;
		}
	}

	if (bit_shift > 0) {
		uint32_t carry = 0;
		for (int i = 0; i < BIGINT_DENSE_LIMBS; i++) {
			uint32_t next_carry = bi->limbs32[i] >> (32 - bit_shift);
			bi->limbs32[i] = (bi->limbs32[i] << bit_shift) | carry;
			carry = next_carry;
		}
	}

	bi->limb_count = BIGINT_DENSE_LIMBS;
	bigint_normalize(bi);
	return 0;
}

int bigint_shift_right(BigInt *bi, int bits) {
	if (!bi || bits < 0) return -1;
	if (bits == 0) return 0;

	int word_shift = bits / 32;
	int bit_shift = bits % 32;

	if (word_shift > 0) {
		for (int i = 0; i + word_shift < BIGINT_DENSE_LIMBS; i++) {
			bi->limbs32[i] = bi->limbs32[i + word_shift];
		}
		for (int i = BIGINT_DENSE_LIMBS - word_shift; i < BIGINT_DENSE_LIMBS; i++) {
			if (i >= 0) bi->limbs32[i] = 0;
		}
	}

	if (bit_shift > 0) {
		uint32_t carry = 0;
		for (int i = BIGINT_DENSE_LIMBS - 1; i >= 0; i--) {
			uint32_t next_carry = bi->limbs32[i] << (32 - bit_shift);
			bi->limbs32[i] = (bi->limbs32[i] >> bit_shift) | carry;
			carry = next_carry;
		}
	}

	bigint_normalize(bi);
	return 0;
}

int bigint_bitwise_and(BigInt *result, const BigInt *a, const BigInt *b) {
	if (!result || !a || !b) return -1;
	int maxLen = a->limb_count > b->limb_count ? a->limb_count : b->limb_count;
	for (int i = 0; i < maxLen; i++) {
		uint32_t va = i < a->limb_count ? a->limbs32[i] : 0;
		uint32_t vb = i < b->limb_count ? b->limbs32[i] : 0;
		result->limbs32[i] = va & vb;
	}
	result->limb_count = maxLen;
	result->flags = FMT_DENSE_32;
	bigint_normalize(result);
	return 0;
}

int bigint_bitwise_or(BigInt *result, const BigInt *a, const BigInt *b) {
	if (!result || !a || !b) return -1;
	int maxLen = a->limb_count > b->limb_count ? a->limb_count : b->limb_count;
	for (int i = 0; i < maxLen; i++) {
		uint32_t va = i < a->limb_count ? a->limbs32[i] : 0;
		uint32_t vb = i < b->limb_count ? b->limbs32[i] : 0;
		result->limbs32[i] = va | vb;
	}
	result->limb_count = maxLen;
	result->flags = FMT_DENSE_32;
	bigint_normalize(result);
	return 0;
}

int bigint_bitwise_xor(BigInt *result, const BigInt *a, const BigInt *b) {
	if (!result || !a || !b) return -1;
	int maxLen = a->limb_count > b->limb_count ? a->limb_count : b->limb_count;
	for (int i = 0; i < maxLen; i++) {
		uint32_t va = i < a->limb_count ? a->limbs32[i] : 0;
		uint32_t vb = i < b->limb_count ? b->limbs32[i] : 0;
		result->limbs32[i] = va ^ vb;
	}
	result->limb_count = maxLen;
	result->flags = FMT_DENSE_32;
	bigint_normalize(result);
	return 0;
}

void bigint_normalize(BigInt *bi) {
	if (!bi) return;
	while (bi->limb_count > 1 && bi->limbs32[bi->limb_count - 1] == 0) {
		bi->limb_count--;
	}
	bi->flags |= FMT_NORMALIZED;
}

int bigint_bit_length(const BigInt *bi) {
	if (!bi || bi->limb_count == 0) return 0;

	int top = bi->limb_count - 1;
	while (top > 0 && bi->limbs32[top] == 0) top--;

	uint32_t topVal = bi->limbs32[top];
	int bits = top * 32;
	while (topVal > 0) {
		bits++;
		topVal >>= 1;
	}
	return bits;
}

int bigint_karatsuba(BigInt *result, const BigInt *a, const BigInt *b) {
	if (!result || !a || !b) return -1;

	if (a->limb_count <= 4 || b->limb_count <= 4) {
		return bigint_multiply(result, a, b);
	}

	int half = (a->limb_count > b->limb_count ? a->limb_count : b->limb_count) / 2;

	BigInt a_low, a_high, b_low, b_high;
	bigint_init(&a_low);
	bigint_init(&a_high);
	bigint_init(&b_low);
	bigint_init(&b_high);

	for (int i = 0; i < half && i < a->limb_count; i++)
		a_low.limbs32[i] = a->limbs32[i];
	a_low.limb_count = half < a->limb_count ? half : a->limb_count;

	for (int i = half; i < a->limb_count; i++)
		a_high.limbs32[i - half] = a->limbs32[i];
	a_high.limb_count = a->limb_count > half ? a->limb_count - half : 0;
	if (a_high.limb_count == 0) a_high.limb_count = 1;

	for (int i = 0; i < half && i < b->limb_count; i++)
		b_low.limbs32[i] = b->limbs32[i];
	b_low.limb_count = half < b->limb_count ? half : b->limb_count;

	for (int i = half; i < b->limb_count; i++)
		b_high.limbs32[i - half] = b->limbs32[i];
	b_high.limb_count = b->limb_count > half ? b->limb_count - half : 0;
	if (b_high.limb_count == 0) b_high.limb_count = 1;

	bigint_normalize(&a_low);
	bigint_normalize(&a_high);
	bigint_normalize(&b_low);
	bigint_normalize(&b_high);

	BigInt z0, z2, z1;
	BigInt a_sum, b_sum, z1_full;
	bigint_init(&z0);
	bigint_init(&z2);
	bigint_init(&z1);
	bigint_init(&a_sum);
	bigint_init(&b_sum);
	bigint_init(&z1_full);

	bigint_multiply(&z0, &a_low, &b_low);
	bigint_multiply(&z2, &a_high, &b_high);

	bigint_add(&a_sum, &a_low, &a_high);
	bigint_add(&b_sum, &b_low, &b_high);
	bigint_multiply(&z1_full, &a_sum, &b_sum);

	bigint_subtract(&z1, &z1_full, &z0);
	bigint_subtract(&z1, &z1, &z2);

	bigint_shift_left(&z1, half * 32);
	bigint_shift_left(&z2, half * 32 * 2);

	bigint_init(result);
	bigint_add(result, &z0, &z1);
	bigint_add(result, result, &z2);

	return 0;
}

int bigint_mul_optimized(BigInt *result, const BigInt *a, const BigInt *b) {
	if (!result || !a || !b) return -1;

	if (a->flags & FMT_OPTIMIZED == 0) {
		return bigint_multiply(result, a, b);
	}

	SparseBigInt sa, sb, sr;
	sparse_bigint_init(&sa);
	sparse_bigint_init(&sb);
	sparse_bigint_init(&sr);

	bigint_to_sparse(&sa, a);
	bigint_to_sparse(&sb, b);
	sparse_bigint_multiply(&sr, &sa, &sb);
	sparse_to_bigint(result, &sr);
	return 0;
}

void sparse_bigint_init(SparseBigInt *sbi) {
	memset(sbi, 0, sizeof(SparseBigInt));
	sbi->flags = FMT_SPARSE_64;
	sbi->allocated_size = sizeof(SparseBigInt);
}

int sparse_bigint_multiply(SparseBigInt *result, const SparseBigInt *a, const SparseBigInt *b) {
	if (!result || !a || !b) return -1;

	sparse_bigint_init(result);

	for (int i = 0; i < a->nonzero_count; i++) {
		int ai = a->nonzero_indices[i];
		for (int j = 0; j < b->nonzero_count; j++) {
			int bj = b->nonzero_indices[j];
			int ri = ai + bj;
			if (ri >= BIGINT_SPARSE_LIMBS) continue;

			__uint128_t prod = (__uint128_t)a->limbs64[ai] * (__uint128_t)b->limbs64[bj];
			result->limbs64[ri] += (uint64_t)prod;

			if (ri + 1 < BIGINT_SPARSE_LIMBS) {
				result->limbs64[ri + 1] += (uint64_t)(prod >> 64);
			}
		}
	}

	result->nonzero_count = 0;
	for (int i = 0; i < BIGINT_SPARSE_LIMBS; i++) {
		if (result->limbs64[i] != 0) {
			result->nonzero_indices[result->nonzero_count++] = i;
		}
	}

	result->limb_count = BIGINT_SPARSE_LIMBS;
	return 0;
}

int bigint_to_sparse(SparseBigInt *dst, const BigInt *src) {
	if (!dst || !src) return -1;
	sparse_bigint_init(dst);

	int pairs = (src->limb_count + 1) / 2;
	if (pairs > BIGINT_SPARSE_LIMBS) pairs = BIGINT_SPARSE_LIMBS;

	for (int i = 0; i < pairs; i++) {
		uint64_t low = src->limbs32[i * 2];
		uint64_t high = (i * 2 + 1 < src->limb_count) ? src->limbs32[i * 2 + 1] : 0;
		dst->limbs64[i] = (high << 32) | low;
		if (dst->limbs64[i] != 0) {
			dst->nonzero_indices[dst->nonzero_count++] = i;
		}
	}

	dst->limb_count = pairs;
	return 0;
}

int sparse_to_bigint(BigInt *dst, const SparseBigInt *src) {
	if (!dst || !src) return -1;
	bigint_init(dst);

	for (int i = 0; i < src->limb_count && i < BIGINT_SPARSE_LIMBS; i++) {
		int lo_idx = i * 2;
		int hi_idx = i * 2 + 1;
		if (lo_idx < BIGINT_DENSE_LIMBS)
			dst->limbs32[lo_idx] = (uint32_t)(src->limbs64[i] & 0xFFFFFFFF);
		if (hi_idx < BIGINT_DENSE_LIMBS)
			dst->limbs32[hi_idx] = (uint32_t)(src->limbs64[i] >> 32);
	}

	dst->limb_count = src->limb_count * 2;
	if (dst->limb_count > BIGINT_DENSE_LIMBS) dst->limb_count = BIGINT_DENSE_LIMBS;
	bigint_normalize(dst);
	return 0;
}

uint32_t bigint_crc32(const BigInt *bi) {
	if (!bi) return 0;
	uint32_t crc = 0xFFFFFFFF;
	const uint8_t *data = (const uint8_t *)bi->limbs32;
	size_t len = (size_t)bi->limb_count * 4;
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320;
			else
				crc = crc >> 1;
		}
	}
	return ~crc;
}

int bigint_gcd(BigInt *result, const BigInt *a, const BigInt *b) {
	if (!result || !a || !b) return -1;

	BigInt x, y, tmp_q, tmp_r;
	bigint_copy(&x, a);
	bigint_copy(&y, b);

	while (!bigint_is_zero(&y)) {
		bigint_divide(&tmp_q, &tmp_r, &x, &y);
		bigint_copy(&x, &y);
		bigint_copy(&y, &tmp_r);
	}

	bigint_copy(result, &x);
	return 0;
}

int bigint_mod_inverse(BigInt *result, const BigInt *a, const BigInt *mod) {
	if (!result || !a || !mod) return -1;

	BigInt old_r, r, old_s, s;
	BigInt q, tmp, tmp2;

	bigint_copy(&old_r, a);
	bigint_copy(&r, mod);
	bigint_set_one(&old_s);
	bigint_set_zero(&s);

	while (!bigint_is_zero(&r)) {
		bigint_init(&q);
		bigint_init(&tmp);
		bigint_divide(&q, &tmp, &old_r, &r);

		bigint_copy(&old_r, &r);
		bigint_copy(&r, &tmp);

		bigint_init(&tmp2);
		bigint_multiply(&tmp2, &q, &s);
		bigint_subtract(&tmp, &old_s, &tmp2);

		bigint_copy(&old_s, &s);
		bigint_copy(&s, &tmp);
	}

	if (!bigint_is_one(&old_r)) return -1;

	if (old_s.flags & FMT_NEGATIVE) {
		bigint_add(&old_s, &old_s, mod);
	}

	bigint_copy(result, &old_s);
	return 0;
}

void bigint_swap(BigInt *a, BigInt *b) {
	if (!a || !b) return;
	BigInt tmp;
	memcpy(&tmp, a, sizeof(BigInt));
	memcpy(a, b, sizeof(BigInt));
	memcpy(b, &tmp, sizeof(BigInt));
}

int bigint_from_uint64(BigInt *bi, uint64_t val) {
	if (!bi) return -1;
	bigint_init(bi);
	bi->limbs32[0] = (uint32_t)(val & 0xFFFFFFFF);
	bi->limbs32[1] = (uint32_t)(val >> 32);
	bi->limb_count = bi->limbs32[1] ? 2 : 1;
	return 0;
}

uint64_t bigint_to_uint64(const BigInt *bi) {
	if (!bi) return 0;
	uint64_t result = bi->limbs32[0];
	if (bi->limb_count > 1)
		result |= ((uint64_t)bi->limbs32[1]) << 32;
	return result;
}

int bigint_random(BigInt *bi, int bit_length, uint32_t seed) {
	if (!bi || bit_length <= 0) return -1;
	bigint_init(bi);

	int numLimbs = (bit_length + 31) / 32;
	if (numLimbs > BIGINT_DENSE_LIMBS) numLimbs = BIGINT_DENSE_LIMBS;

	uint32_t state = seed;
	for (int i = 0; i < numLimbs; i++) {
		state = state * 1103515245 + 12345;
		bi->limbs32[i] = state;
	}

	int topBits = bit_length % 32;
	if (topBits > 0 && numLimbs > 0) {
		bi->limbs32[numLimbs - 1] &= (1U << topBits) - 1;
		bi->limbs32[numLimbs - 1] |= (1U << (topBits - 1));
	}

	bi->limb_count = numLimbs;
	bigint_normalize(bi);
	return 0;
}

static int bigint_schoolbook_square(BigInt *result, const BigInt *a) {
	if (!result || !a) return -1;

	BigInt tmp;
	bigint_init(&tmp);

	for (int i = 0; i < a->limb_count; i++) {
		uint64_t carry = 0;
		uint64_t sq = (uint64_t)a->limbs32[i] * (uint64_t)a->limbs32[i];
		int k = 2 * i;
		if (k < BIGINT_DENSE_LIMBS) {
			sq += tmp.limbs32[k];
			tmp.limbs32[k] = (uint32_t)(sq & 0xFFFFFFFF);
			carry = sq >> 32;
		}

		for (int j = i + 1; j < a->limb_count; j++) {
			k = i + j;
			if (k >= BIGINT_DENSE_LIMBS) break;
			uint64_t prod = 2 * (uint64_t)a->limbs32[i] * (uint64_t)a->limbs32[j];
			prod += tmp.limbs32[k];
			prod += carry;
			tmp.limbs32[k] = (uint32_t)(prod & 0xFFFFFFFF);
			carry = prod >> 32;
		}
		k = i + a->limb_count;
		if (k < BIGINT_DENSE_LIMBS) {
			tmp.limbs32[k] += (uint32_t)carry;
		}
	}

	tmp.limb_count = 2 * a->limb_count;
	if (tmp.limb_count > BIGINT_DENSE_LIMBS) tmp.limb_count = BIGINT_DENSE_LIMBS;
	bigint_normalize(&tmp);
	bigint_copy(result, &tmp);
	return 0;
}

static int bigint_montgomery_reduce(BigInt *result, const BigInt *t, const BigInt *mod,
                                    uint32_t mod_inv) {
	if (!result || !t || !mod) return -1;

	BigInt tmp;
	bigint_copy(&tmp, t);
	int n = mod->limb_count;

	for (int i = 0; i < n; i++) {
		uint32_t u = tmp.limbs32[i] * mod_inv;
		uint64_t carry = 0;
		for (int j = 0; j < n; j++) {
			int k = i + j;
			if (k >= BIGINT_DENSE_LIMBS) break;
			uint64_t prod = (uint64_t)u * (uint64_t)mod->limbs32[j];
			prod += tmp.limbs32[k];
			prod += carry;
			tmp.limbs32[k] = (uint32_t)(prod & 0xFFFFFFFF);
			carry = prod >> 32;
		}
		int k = i + n;
		while (carry && k < BIGINT_DENSE_LIMBS) {
			uint64_t sum = (uint64_t)tmp.limbs32[k] + carry;
			tmp.limbs32[k] = (uint32_t)(sum & 0xFFFFFFFF);
			carry = sum >> 32;
			k++;
		}
	}

	for (int i = 0; i < n && i + n < BIGINT_DENSE_LIMBS; i++) {
		result->limbs32[i] = tmp.limbs32[i + n];
	}
	result->limb_count = n;
	bigint_normalize(result);

	if (bigint_compare(result, mod) >= 0) {
		bigint_subtract(result, result, mod);
	}
	return 0;
}

static uint32_t compute_mod_inverse_32(uint32_t mod0) {
	uint32_t inv = 1;
	for (int i = 0; i < 31; i++) {
		inv = inv * (2 - mod0 * inv);
	}
	return (uint32_t)(-(int32_t)inv);
}

static int bigint_abs_compare(const BigInt *a, const BigInt *b) {
	int la = a->limb_count;
	int lb = b->limb_count;
	while (la > 1 && a->limbs32[la - 1] == 0) la--;
	while (lb > 1 && b->limbs32[lb - 1] == 0) lb--;
	if (la != lb) return la > lb ? 1 : -1;
	for (int i = la - 1; i >= 0; i--) {
		if (a->limbs32[i] > b->limbs32[i]) return 1;
		if (a->limbs32[i] < b->limbs32[i]) return -1;
	}
	return 0;
}

static void bigint_add_in_place(BigInt *a, const BigInt *b) {
	uint64_t carry = 0;
	int maxLen = a->limb_count > b->limb_count ? a->limb_count : b->limb_count;
	for (int i = 0; i < maxLen; i++) {
		uint64_t sum = carry;
		if (i < a->limb_count) sum += a->limbs32[i];
		if (i < b->limb_count) sum += b->limbs32[i];
		a->limbs32[i] = (uint32_t)(sum & 0xFFFFFFFF);
		carry = sum >> 32;
	}
	if (carry && maxLen < BIGINT_DENSE_LIMBS) {
		a->limbs32[maxLen] = (uint32_t)carry;
		a->limb_count = maxLen + 1;
	} else {
		a->limb_count = maxLen;
	}
}

static int bigint_mod(BigInt *result, const BigInt *a, const BigInt *mod) {
	BigInt q;
	bigint_init(&q);
	return bigint_divide(&q, result, a, mod);
}

static int bigint_mul_limb(BigInt *result, const BigInt *a, uint32_t limb) {
	if (!result || !a) return -1;
	bigint_init(result);
	uint64_t carry = 0;
	for (int i = 0; i < a->limb_count; i++) {
		uint64_t prod = (uint64_t)a->limbs32[i] * limb + carry;
		result->limbs32[i] = (uint32_t)(prod & 0xFFFFFFFF);
		carry = prod >> 32;
	}
	result->limb_count = a->limb_count;
	if (carry && result->limb_count < BIGINT_DENSE_LIMBS) {
		result->limbs32[result->limb_count] = (uint32_t)carry;
		result->limb_count++;
	}
	bigint_normalize(result);
	return 0;
}

static bool bigint_is_even(const BigInt *bi) {
	if (!bi || bi->limb_count == 0) return true;
	return (bi->limbs32[0] & 1) == 0;
}

static int bigint_half(BigInt *bi) {
	return bigint_shift_right(bi, 1);
}

static int bigint_double_val(BigInt *bi) {
	return bigint_shift_left(bi, 1);
}

static uint32_t bigint_get_bit(const BigInt *bi, int bit_pos) {
	if (!bi) return 0;
	int limb_idx = bit_pos / 32;
	int bit_idx = bit_pos % 32;
	if (limb_idx >= bi->limb_count) return 0;
	return (bi->limbs32[limb_idx] >> bit_idx) & 1;
}

static void bigint_set_bit(BigInt *bi, int bit_pos) {
	if (!bi) return;
	int limb_idx = bit_pos / 32;
	int bit_idx = bit_pos % 32;
	if (limb_idx >= BIGINT_DENSE_LIMBS) return;
	bi->limbs32[limb_idx] |= (1U << bit_idx);
	if (limb_idx >= bi->limb_count)
		bi->limb_count = limb_idx + 1;
}

static void bigint_clear_bit(BigInt *bi, int bit_pos) {
	if (!bi) return;
	int limb_idx = bit_pos / 32;
	int bit_idx = bit_pos % 32;
	if (limb_idx >= bi->limb_count) return;
	bi->limbs32[limb_idx] &= ~(1U << bit_idx);
}

static int bigint_count_trailing_zeros(const BigInt *bi) {
	if (!bi) return 0;
	int total = 0;
	for (int i = 0; i < bi->limb_count; i++) {
		if (bi->limbs32[i] == 0) {
			total += 32;
		} else {
			uint32_t v = bi->limbs32[i];
			while (!(v & 1)) {
				v >>= 1;
				total++;
			}
			break;
		}
	}
	return total;
}

static int bigint_leading_zeros(uint32_t v) {
	if (v == 0) return 32;
	int n = 0;
	if ((v & 0xFFFF0000) == 0) { n += 16; v <<= 16; }
	if ((v & 0xFF000000) == 0) { n += 8; v <<= 8; }
	if ((v & 0xF0000000) == 0) { n += 4; v <<= 4; }
	if ((v & 0xC0000000) == 0) { n += 2; v <<= 2; }
	if ((v & 0x80000000) == 0) { n += 1; }
	return n;
}

static int bigint_popcount(const BigInt *bi) {
	if (!bi) return 0;
	int count = 0;
	for (int i = 0; i < bi->limb_count; i++) {
		uint32_t v = bi->limbs32[i];
		while (v) {
			count += v & 1;
			v >>= 1;
		}
	}
	return count;
}
