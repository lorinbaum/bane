#include "bane.h"
#include <stdio.h>

// lookup-table for the 4 different sequence lengths 
static const struct {
	uint_least8_t lower;  // lower bound of sequence first byte
	uint_least8_t upper;  // upper bound of sequence first byte
	uint_least32_t mincp; // smallest non-overlong encoded codepoint
	uint_least32_t maxcp; // largest encodable codepoint
			      		  // implicit: index in lut + 1 equals sequence length
} lut[] = {
	{0x00, 0x7F, UINT32_C(0),       (UINT32_C(1) <<  7) - 1},
	{0xC0, 0xDF, UINT32_C(1) << 7,  (UINT32_C(1) << 11) - 1},
	{0xE0, 0xEF, UINT32_C(1) << 11, (UINT32_C(1) << 16) - 1},
	{0xF0, 0xF7, UINT32_C(1) << 16, (UINT32_C(1) << 21) - 1}
};

static bool is_valid_utf8_codepoint(uint32_t codepoint) {
	return !between(codepoint, UINT32_C(0xD800), UINT32_C(0xDFFF)) && // U+D800 through U+DFFF are reserved for UTF-16 (see RFC-3629)
	codepoint <= 0x10FFFF; // max utf-8 codepoint
}

static bool is_continuation_byte(unsigned char c) { return between(c, 0x80, 0xBF); }

typedef enum { SEQ_OK = 0, SEQ_TRUNCATED = 1, SEQ_INVALID = 2 } SEQStatus;

static SEQStatus utf8_sequence_length(const unsigned char *str, size_t max_len, uint_least8_t *sequence_len) {
	*sequence_len = 0;
	for (uint_least8_t off = 0; off < 4; off++) {
		if (between(str[0], lut[off].lower, lut[off].upper)) {
			*sequence_len = off + 1;
			break;
		}
	}
	if (*sequence_len == 0) {
		*sequence_len = 1; // invalid start of sequence
		return SEQ_INVALID;
	}
	if (*sequence_len > max_len) { return SEQ_TRUNCATED; }
	for (uint_least8_t i = 1; i < *sequence_len; i++) {
		if (!is_continuation_byte(str[i])) {
			*sequence_len = i;
			return SEQ_INVALID;
		}
	}
	return SEQ_OK;
}

static bool utf8_codepoint_bytes(uint32_t codepoint, uint_least8_t *len) {
	if (is_valid_utf8_codepoint(codepoint)) {
		for (uint_least8_t off = 0; off < 4; off++) {
			if (between(codepoint, lut[off].mincp, lut[off].maxcp)) {
				*len = off + 1;
				return true;
			}
		}
	}
	*len = 3; // invalid codepoints are encoded as U+FFFD (3 bytes)
	return false;
}

UTF8Status utf8_measure_codepoints(const char *str, size_t in_len, size_t *out_len, size_t *processed_bytes) {
	if (str == NULL || out_len == NULL || processed_bytes == NULL)  { return UTF8_INVALID_ARGUMENT; }
	*out_len = 0;
	uint_least8_t sequence_len;
	for (*processed_bytes = 0; *processed_bytes < in_len; *processed_bytes += sequence_len, (*out_len)++) {
		SEQStatus seq_status = utf8_sequence_length((const unsigned char *) str + *processed_bytes, min(4U, in_len - *processed_bytes), &sequence_len);
		// invalid sequences aren't handled here because the validation in utf8_sequence_length is incomplete. decoded codepoints could be invalid.
		if (seq_status == SEQ_TRUNCATED) { return UTF8_TOO_SHORT; }
	}
	return UTF8_OK;
}

UTF8Status utf8_decode(const char *str, size_t in_len, bool strict, size_t out_cap, uint32_t *codepoints, size_t *out_len) {
	if (str == NULL || out_len == NULL || codepoints == NULL) { return UTF8_INVALID_ARGUMENT; }
	*out_len = 0;
	UTF8Status status = UTF8_OK;
	uint_least8_t sequence_len;
	for (size_t i = 0; i < in_len && *out_len < out_cap; i += sequence_len, (*out_len)++) {
		uint32_t *codepoint = codepoints + *out_len;
		const unsigned char *ustr = (const unsigned char *) (str + i);
		SEQStatus seq_status = utf8_sequence_length(ustr, min(4U, in_len - i), &sequence_len);
		if (seq_status == SEQ_OK) {
			*codepoint = ustr[0] - lut[sequence_len - 1].lower;
			for (uint_least8_t j = 1; j < sequence_len; j++) { *codepoint = (*codepoint << 6) | (ustr[j] & 0x3F); }
			if (*codepoint < lut[sequence_len - 1].mincp || !is_valid_utf8_codepoint(*codepoint)) { seq_status = SEQ_INVALID; }
		}
		if (seq_status == SEQ_TRUNCATED) { return UTF8_TOO_SHORT; }
		else if (seq_status == SEQ_INVALID) {
			status = UTF8_INVALID;
			if (strict) { return status; }
			*codepoint = INVALID_CODEPOINT;
		}
	}
	return status;
}

UTF8Status utf8_measure_bytes(const uint32_t *codepoints, size_t in_len, bool strict, size_t *out_len) {
	if (codepoints == NULL || out_len == NULL) { return UTF8_INVALID_ARGUMENT; }
	*out_len = 0;
	UTF8Status status = UTF8_OK;
	uint_least8_t sequence_len;
	for (size_t i = 0; i < in_len; i++, *out_len += sequence_len) {
		if (!utf8_codepoint_bytes(codepoints[i], &sequence_len)) {
			status = UTF8_INVALID;
			if (strict) { return status; }
		}
	}
	return status;
}

UTF8Status utf8_encode(const uint32_t *codepoints, size_t in_len, bool strict, size_t out_cap, char *str, size_t *out_len) {
	if (str == NULL || codepoints == NULL || out_len == NULL || out_cap < 1 ) { return UTF8_INVALID_ARGUMENT; } // outcap >= 1 because at least space for '\0'
	*out_len = 0;
	UTF8Status status = UTF8_OK;
	uint_least8_t sequence_len;
	for (size_t i = 0; i < in_len; i++, *out_len += sequence_len) {
		uint32_t codepoint = codepoints[i];
		bool valid = utf8_codepoint_bytes(codepoint, &sequence_len);
		if (sequence_len > min(4U, out_cap - 1 - *out_len)) {
			status = UTF8_TOO_SHORT;
			goto terminate;
		}
		if (!valid) {
			status = UTF8_INVALID;
			if (strict) { goto terminate; }
			codepoint = INVALID_CODEPOINT;
		}
		unsigned char *ustr = (unsigned char*) (str + *out_len);
		/* lut[off].lower acts as the bit-format for the first byte. Each continuation bytes stores 6 bits of the codepoint
		Earlier validity checks ensure that there enough total bits available. */
		ustr[0] = lut[sequence_len - 1].lower | (uint_least8_t)(codepoint >> (6 * (sequence_len - 1))); // starting bytes
		for (uint_least8_t j = 1; j < sequence_len; j++) { 										        // continuation bytes
			ustr[j] = 0x80 | ((codepoint >> (6 * (sequence_len - 1 - j))) & 0x3F);
		}
	}
terminate:
	str[*out_len] = '\0';
	return status;
}