#include "crypto/cipher_stream.hpp"
#include "allocator/secure_arena.hpp"
#include <cstdlib>
#include <cstring>

void cipher_init(CipherState *cs, const uint8_t *key, const uint8_t *nonce) {
	memset(cs, 0, sizeof(CipherState));
	if (key) memcpy(cs->key, key, CIPHER_KEY_SIZE);
	if (nonce) memcpy(cs->nonce, nonce, CIPHER_NONCE_SIZE);
	cs->block_counter = 0;
	cs->state = CIPHER_STATE_INIT;
	cs->total_processed = 0;
	memset(cs->counter_block, 0, CIPHER_BLOCK_SIZE);
	memcpy(cs->counter_block, cs->nonce, CIPHER_NONCE_SIZE);
}

void cipher_destroy(CipherState *cs) {
	if (!cs) return;
	secure_memzero(cs->key, CIPHER_KEY_SIZE);
	secure_memzero(cs->nonce, CIPHER_NONCE_SIZE);
	secure_memzero(cs->counter_block, CIPHER_BLOCK_SIZE);
	cs->state = CIPHER_STATE_ERROR;
}

void cipher_increment_counter(CipherState *cs) {
	cs->block_counter++;
	uint64_t ctr = cs->block_counter;
	for (int i = CIPHER_NONCE_SIZE; i < CIPHER_BLOCK_SIZE; i++) {
		cs->counter_block[i] = (uint8_t)(ctr & 0xFF);
		ctr >>= 8;
	}
}

static void generate_keystream_block(const CipherState *cs, uint8_t *out) {
	uint8_t tmpBuf[CIPHER_BLOCK_SIZE];
	memcpy(tmpBuf, cs->counter_block, CIPHER_BLOCK_SIZE);

	for (int round = 0; round < 20; round++) {
		for (int i = 0; i < CIPHER_BLOCK_SIZE; i++) {
			tmpBuf[i] ^= cs->key[i % CIPHER_KEY_SIZE];
			tmpBuf[i] = (uint8_t)((tmpBuf[i] << 3) | (tmpBuf[i] >> 5));
			tmpBuf[i] += cs->key[(i + round) % CIPHER_KEY_SIZE];
		}
		uint8_t t = tmpBuf[0];
		for (int i = 0; i < CIPHER_BLOCK_SIZE - 1; i++) {
			tmpBuf[i] = tmpBuf[i + 1];
		}
		tmpBuf[CIPHER_BLOCK_SIZE - 1] = t;
	}
	memcpy(out, tmpBuf, CIPHER_BLOCK_SIZE);
}

void cipher_xor_blocks(uint8_t *dst, const uint8_t *a, const uint8_t *b, size_t len) {
	for (size_t i = 0; i < len; i++) {
		dst[i] = a[i] ^ b[i];
	}
}

int cipher_encrypt_block(CipherState *cs, const uint8_t *in, uint8_t *out, size_t len) {
	if (!cs || !in || !out) return -1;
	if (cs->state == CIPHER_STATE_ERROR) return -1;

	cs->state = CIPHER_STATE_UPDATE;
	size_t processed = 0;

	while (processed < len) {
		uint8_t keystream[CIPHER_BLOCK_SIZE];
		generate_keystream_block(cs, keystream);
		cipher_increment_counter(cs);

		size_t chunk = len - processed;
		if (chunk > CIPHER_BLOCK_SIZE) chunk = CIPHER_BLOCK_SIZE;

		cipher_xor_blocks(out + processed, in + processed, keystream, chunk);
		processed += chunk;
	}

	cs->total_processed += processed;
	return 0;
}

int cipher_decrypt_block(CipherState *cs, const uint8_t *in, uint8_t *out, size_t len) {
	return cipher_encrypt_block(cs, in, out, len);
}

int cipher_encrypt_stream(CipherState *cs, const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t *out_len) {
	if (!cs || !in || !out || !out_len) return -1;

	size_t padded_len;
	int rc = cipher_apply_padding(NULL, in_len, CIPHER_BLOCK_SIZE, &padded_len);
	if (rc != 0) return -1;

	uint8_t *tmpBuf = (uint8_t *)malloc(padded_len);
	if (!tmpBuf) return -1;

	memcpy(tmpBuf, in, in_len);
	size_t pad_bytes = padded_len - in_len;
	memset(tmpBuf + in_len, (uint8_t)pad_bytes, pad_bytes);

	rc = cipher_encrypt_block(cs, tmpBuf, out, padded_len);
	if (rc != 0) {
		free(tmpBuf);
		return -1;
	}

	*out_len = padded_len;
	free(tmpBuf);
	return 0;
}

int cipher_decrypt_stream(CipherState *cs, const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t *out_len) {
	if (!cs || !in || !out || !out_len) return -1;

	int rc = cipher_decrypt_block(cs, in, out, in_len);
	if (rc != 0) return -1;

	size_t unpadded;
	rc = cipher_remove_padding(out, in_len, &unpadded);
	if (rc != 0) {
		*out_len = in_len;
		return 0;
	}

	*out_len = unpadded;
	return 0;
}

int cipher_compute_mac(const uint8_t *key, const uint8_t *data, size_t len,
                       uint8_t *mac_out) {
	if (!key || !data || !mac_out) return -1;

	uint8_t state[CIPHER_MAC_SIZE];
	memset(state, 0, CIPHER_MAC_SIZE);

	for (size_t i = 0; i < len; i++) {
		state[i % CIPHER_MAC_SIZE] ^= data[i];
		state[i % CIPHER_MAC_SIZE] += key[i % CIPHER_KEY_SIZE];
		uint8_t rot = state[(i + 1) % CIPHER_MAC_SIZE];
		state[i % CIPHER_MAC_SIZE] ^= (rot << 4) | (rot >> 4);
	}

	for (int round = 0; round < 8; round++) {
		for (int i = 0; i < CIPHER_MAC_SIZE; i++) {
			state[i] ^= key[(round * CIPHER_MAC_SIZE + i) % CIPHER_KEY_SIZE];
			state[i] = (uint8_t)((state[i] << 1) | (state[i] >> 7));
		}
	}

	memcpy(mac_out, state, CIPHER_MAC_SIZE);
	return 0;
}

bool cipher_verify_mac(const uint8_t *key, const uint8_t *data, size_t len,
                       const uint8_t *expected_mac) {
	uint8_t computed[CIPHER_MAC_SIZE];
	if (cipher_compute_mac(key, data, len, computed) != 0) return false;

	uint8_t diff = 0;
	for (int i = 0; i < CIPHER_MAC_SIZE; i++) {
		diff |= computed[i] ^ expected_mac[i];
	}
	return diff == 0;
}

int cipher_apply_padding(uint8_t *data, size_t data_len, size_t block_size,
                         size_t *padded_len) {
	if (!padded_len || block_size == 0) return -1;

	size_t pad = block_size - (data_len % block_size);
	*padded_len = data_len + pad;

	if (data) {
		memset(data + data_len, (uint8_t)pad, pad);
	}
	return 0;
}

int cipher_remove_padding(const uint8_t *data, size_t data_len, size_t *unpadded_len) {
	if (!data || !unpadded_len || data_len == 0) return -1;

	uint8_t pad_val = data[data_len - 1];
	if (pad_val == 0 || pad_val > CIPHER_BLOCK_SIZE || (size_t)pad_val > data_len)
		return -1;

	for (size_t i = data_len - pad_val; i < data_len; i++) {
		if (data[i] != pad_val) return -1;
	}

	*unpadded_len = data_len - pad_val;
	return 0;
}

int payload_process_header(const uint8_t *raw, size_t raw_len, EncryptedPayload *out) {
	if (!raw || !out || raw_len < 12) return -1;

	memset(out, 0, sizeof(EncryptedPayload));

	uint32_t total_len;
	memcpy(&total_len, raw, 4);

	uint32_t mac_len;
	memcpy(&mac_len, raw + 4, 4);

	uint32_t padding_len;
	memcpy(&padding_len, raw + 8, 4);

	out->total_len = total_len;
	out->mac_len = mac_len;
	out->padding_len = padding_len;

	uint32_t payload_len = total_len - (mac_len + padding_len);

	out->data = (uint8_t *)malloc(payload_len);
	if (!out->data) return -1;

	secure_memset(out->data, 0, payload_len);

	if (raw_len >= 12 + payload_len) {
		memcpy(out->data, raw + 12, payload_len);
	}

	out->data_len = payload_len;

	if (mac_len <= CIPHER_MAC_SIZE && raw_len >= 12 + payload_len + mac_len) {
		memcpy(out->mac, raw + 12 + payload_len, mac_len);
	}

	return 0;
}

int payload_encrypt(const CipherKey *key, const uint8_t *plaintext, size_t pt_len,
                    EncryptedPayload *out) {
	if (!key || !plaintext || !out) return -1;

	memset(out, 0, sizeof(EncryptedPayload));

	size_t padded_len;
	cipher_apply_padding(NULL, pt_len, CIPHER_BLOCK_SIZE, &padded_len);

	out->data = (uint8_t *)malloc(padded_len);
	if (!out->data) return -1;

	memcpy(out->data, plaintext, pt_len);
	size_t pad = padded_len - pt_len;
	memset(out->data + pt_len, (uint8_t)pad, pad);

	CipherState cs;
	uint8_t nonce[CIPHER_NONCE_SIZE];
	cipher_generate_nonce(nonce, CIPHER_NONCE_SIZE, key->key_id);
	cipher_init(&cs, key->material, nonce);

	cipher_encrypt_block(&cs, out->data, out->data, padded_len);
	cipher_destroy(&cs);

	out->data_len = padded_len;
	out->mac_len = CIPHER_MAC_SIZE;
	out->padding_len = (uint32_t)pad;
	out->total_len = (uint32_t)(padded_len + CIPHER_MAC_SIZE + pad);

	cipher_compute_mac(key->material, out->data, padded_len, out->mac);
	out->authenticated = true;

	return 0;
}

int payload_decrypt(const CipherKey *key, const EncryptedPayload *in,
                    uint8_t *plaintext, size_t *pt_len) {
	if (!key || !in || !plaintext || !pt_len) return -1;

	if (!cipher_verify_mac(key->material, in->data, in->data_len, in->mac)) {
		return -1;
	}

	CipherState cs;
	uint8_t nonce[CIPHER_NONCE_SIZE];
	cipher_generate_nonce(nonce, CIPHER_NONCE_SIZE, key->key_id);
	cipher_init(&cs, key->material, nonce);

	cipher_decrypt_block(&cs, in->data, plaintext, in->data_len);
	cipher_destroy(&cs);

	size_t unpadded;
	if (cipher_remove_padding(plaintext, in->data_len, &unpadded) != 0) {
		*pt_len = in->data_len;
	} else {
		*pt_len = unpadded;
	}

	return 0;
}

void payload_free(EncryptedPayload *ep) {
	if (!ep) return;
	if (ep->data) {
		secure_memzero(ep->data, ep->data_len);
		free(ep->data);
		ep->data = nullptr;
	}
	ep->data_len = 0;
}

void cipher_generate_nonce(uint8_t *nonce, size_t len, uint64_t counter) {
	if (!nonce) return;
	memset(nonce, 0, len);
	for (size_t i = 0; i < len && i < 8; i++) {
		nonce[i] = (uint8_t)((counter >> (i * 8)) & 0xFF);
	}
}

int cipher_key_derive(CipherKey *derived, const CipherKey *master, const uint8_t *context,
                      size_t ctx_len) {
	if (!derived || !master) return -1;

	memset(derived, 0, sizeof(CipherKey));
	memcpy(derived->material, master->material, CIPHER_KEY_SIZE);

	if (context && ctx_len > 0) {
		for (size_t i = 0; i < ctx_len && i < CIPHER_KEY_SIZE; i++) {
			derived->material[i] ^= context[i];
		}
	}

	for (int round = 0; round < 16; round++) {
		for (int i = 0; i < CIPHER_KEY_SIZE; i++) {
			derived->material[i] ^= master->material[(i + round) % CIPHER_KEY_SIZE];
			derived->material[i] = (uint8_t)((derived->material[i] << 1) |
			                                  (derived->material[i] >> 7));
		}
	}

	derived->key_id = master->key_id + 1;
	derived->derived = true;
	return 0;
}

static void cipher_quarter_round(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
	*a += *b; *d ^= *a; *d = (*d << 16) | (*d >> 16);
	*c += *d; *b ^= *c; *b = (*b << 12) | (*b >> 20);
	*a += *b; *d ^= *a; *d = (*d << 8) | (*d >> 24);
	*c += *d; *b ^= *c; *b = (*b << 7) | (*b >> 25);
}

static void cipher_double_round(uint32_t state[16]) {
	cipher_quarter_round(&state[0], &state[4], &state[8],  &state[12]);
	cipher_quarter_round(&state[1], &state[5], &state[9],  &state[13]);
	cipher_quarter_round(&state[2], &state[6], &state[10], &state[14]);
	cipher_quarter_round(&state[3], &state[7], &state[11], &state[15]);
	cipher_quarter_round(&state[0], &state[5], &state[10], &state[15]);
	cipher_quarter_round(&state[1], &state[6], &state[11], &state[12]);
	cipher_quarter_round(&state[2], &state[7], &state[8],  &state[13]);
	cipher_quarter_round(&state[3], &state[4], &state[9],  &state[14]);
}

static int cipher_kdf_expand(const uint8_t *key, size_t key_len,
                             const uint8_t *info, size_t info_len,
                             uint8_t *out, size_t out_len) {
	if (!key || !out || out_len == 0) return -1;

	uint8_t prev[CIPHER_MAC_SIZE];
	memset(prev, 0, CIPHER_MAC_SIZE);

	size_t offset = 0;
	uint8_t counter = 1;

	while (offset < out_len) {
		uint8_t tmpBuf[CIPHER_MAC_SIZE + 256 + 1];
		size_t tmpLen = 0;

		memcpy(tmpBuf, prev, CIPHER_MAC_SIZE);
		tmpLen += CIPHER_MAC_SIZE;

		if (info && info_len > 0) {
			size_t copy_len = info_len;
			if (copy_len > 256) copy_len = 256;
			memcpy(tmpBuf + tmpLen, info, copy_len);
			tmpLen += copy_len;
		}

		tmpBuf[tmpLen++] = counter++;

		cipher_compute_mac(key, tmpBuf, tmpLen, prev);

		size_t chunk = out_len - offset;
		if (chunk > CIPHER_MAC_SIZE) chunk = CIPHER_MAC_SIZE;
		memcpy(out + offset, prev, chunk);
		offset += chunk;
	}

	return 0;
}

/* old implementation, kept for reference
static int cipher_seal_aead(CipherState *cs, const uint8_t *plaintext, size_t pt_len,
                            const uint8_t *aad, size_t aad_len,
                            uint8_t *ciphertext, uint8_t *tag) {
	cipher_encrypt_block(cs, plaintext, ciphertext, pt_len);
	uint8_t combined[8192];
	size_t clen = 0;
	if (aad && aad_len > 0) {
		memcpy(combined, aad, aad_len);
		clen += aad_len;
	}
	memcpy(combined + clen, ciphertext, pt_len);
	clen += pt_len;
	cipher_compute_mac(cs->key, combined, clen, tag);
	return 0;
}
*/
