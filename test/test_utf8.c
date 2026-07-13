/*
Decoder modified version of https://github.com/FRIGN/libgrapheme/blob/master/test/utf8-decode.c
Encoder modified version of from https://github.com/FRIGN/libgrapheme/blob/master/test/utf8-encode.c

	ISC-License

	Copyright 2019-2025 Laslo Hunhold <dev@frign.de>

	Permission to use, copy, modify, and/or distribute this software for any
	purpose with or without fee is hereby granted, provided that the above
	copyright notice and this permission notice appear in all copies.

	THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <stdio.h>
#include <string.h>

#include "bane.h"
#define LEN(x) (sizeof(x) / sizeof *(x))

static const struct {
	size_t i;				// test number
	char *str;             	// UTF-8 byte sequence
	size_t len;            	// length of UTF-8 byte sequence
	size_t exp_len;        	// expected codepoint length returned 
	uint_least32_t *exp_cp; // expected codepoint returned
	Utf8Error exp_error;
} decoder_test[] = {
	{ // empty sequence
		.i = 0,
		.str = "",
		.len = 0,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_OK
	},
	{ // NULL str
		.i = 1,
		.str = NULL,
		.len = 0,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_INVALID_ARGUMENT
	},
	{ // invalid lead byte
		.i = 2,
		.str = (char *)(unsigned char[]) { 0xFD },
		.len = 1,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // valid 1-byte sequence
		.i = 3,
		.str = (char *)(unsigned char[]) { 0x01 },
		.len = 1,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0x1 },
		.exp_error = UTF8_OK
	},
	{ // valid 2-byte sequence
		.i = 4,
		.str = (char *)(unsigned char[]) { 0xC3, 0xBF },
		.len = 2,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0xFF },
		.exp_error = UTF8_OK
	},
	{ // invalid 2-byte sequence (second byte missing)
		.i = 5,
		.str = (char *)(unsigned char[]) { 0xC3 },
		.len = 1,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 2-byte sequence (second byte malformed)
		.i = 6,
		.str = (char *)(unsigned char[]) { 0xC3, 0xFF },
		.len = 2,
		.exp_len = 2,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT, INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // invalid 2-byte sequence (overlong encoded)
		.i = 7,
		.str = (char *)(unsigned char[]) { 0xC1, 0xBF },
		.len = 2,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // valid 3-byte sequence
		.i = 8,
		.str = (char *)(unsigned char[]) { 0xE0, 0xBF, 0xBF },
		.len = 3,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0xFFF },
		.exp_error = UTF8_OK
	},
	{ // invalid 3-byte sequence (second byte missing)
		.i = 9,
		.str = (char *)(unsigned char[]) { 0xE0 },
		.len = 1,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 3-byte sequence (second byte malformed)
		.i = 10,
		.str = (char *)(unsigned char[]) { 0xE0, 0x7F, 0xBF },
		.len = 3,
		.exp_len = 3,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT, 0x7F, INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // invalid 3-byte sequence (short string, second byte malformed)
		.i = 11,
		.str = (char *)(unsigned char[]) { 0xE0, 0x7F },
		.len = 2,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 3-byte sequence (third byte missing)
		.i = 12,
		.str = (char *)(unsigned char[]) { 0xE0, 0xBF },
		.len = 2,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 3-byte sequence (third byte malformed)
		.i = 13,
		.str = (char *)(unsigned char[]) { 0xE0, 0xBF, 0x7F },
		.len = 3,
		.exp_len = 2,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT, 0x7F },
		.exp_error = UTF8_INVALID
	},
	{ // invalid 3-byte sequence (overlong encoded)
		.i = 14,
		.str = (char *)(unsigned char[]) { 0xE0, 0x9F, 0xBF },
		.len = 3,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // invalid 3-byte sequence (UTF-16 surrogate half)
		.i = 15,
		.str = (char *)(unsigned char[]) { 0xED, 0xA0, 0x80 },
		.len = 3,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // valid 4-byte sequence
		.i = 16,
		.str = (char *)(unsigned char[]) { 0xF3, 0xBF, 0xBF, 0xBF },
		.len = 4,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0xFFFFF },
		.exp_error = UTF8_OK
	},
	{ // invalid 4-byte sequence (second byte missing)
		.i = 17,
		.str = (char *)(unsigned char[]) { 0xF3 },
		.len = 1,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 4-byte sequence (second byte malformed)
		.i = 18,
		.str = (char *)(unsigned char[]) { 0xF3, 0x7F, 0xBF, 0xBF },
		.len = 4,
		.exp_len = 4,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT, 0x7F, INVALID_CODEPOINT, INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // invalid 4-byte sequence (short string 1, second byte malformed)
		.i = 19,
		.str = (char *)(unsigned char[]) { 0xF3, 0x7F },
		.len = 2,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 4-byte sequence (short string 2, second byte malformed)
		.i = 20,
		.str = (char *)(unsigned char[]) { 0xF3, 0x7F, 0xBF },
		.len = 3,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 4-byte sequence (third byte missing)
		.i = 21,
		.str = (char *)(unsigned char[]) { 0xF3, 0xBF },
		.len = 2,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 4-byte sequence (third byte malformed)
		.i = 22,
		.str = (char *)(unsigned char[]) { 0xF3, 0xBF, 0x7F, 0xBF },
		.len = 4,
		.exp_len = 3,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT, 0x7F, INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // invalid 4-byte sequence (short string, third byte malformed)
		.i = 23,
		.str = (char *)(unsigned char[]) { 0xF3, 0xBF, 0x7F },
		.len = 3,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 4-byte sequence (fourth byte missing)
		.i = 24,
		.str = (char *)(unsigned char[]) { 0xF3, 0xBF, 0xBF },
		.len = 3,
		.exp_len = 0,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0 },
		.exp_error = UTF8_TOO_SHORT
	},
	{ // invalid 4-byte sequence (fourth byte malformed)
		.i = 25,
		.str = (char *)(unsigned char[]) { 0xF3, 0xBF, 0xBF, 0x7F },
		.len = 4,
		.exp_len = 2,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT, 0x7F },
		.exp_error = UTF8_INVALID
	},
	{ // invalid 4-byte sequence (overlong encoded)
		.i = 26,
		.str = (char *)(unsigned char[]) { 0xF0, 0x80, 0x81, 0xBF },
		.len = 4,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // invalid 4-byte sequence (UTF-16-unrepresentable)
		.i = 27,
		.str = (char *)(unsigned char[]) { 0xF4, 0x90, 0x80, 0x80 },
		.len = 4,
		.exp_len = 1,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // invalid sequence, only continuation bytes
		.i = 27,
		.str = (char *)(unsigned char[]) { 0x80, 0x81, 0x82, 0xBF, 0xB0, 0xA0, 0xAA },
		.len = 7,
		.exp_len = 7,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { INVALID_CODEPOINT, INVALID_CODEPOINT, INVALID_CODEPOINT, INVALID_CODEPOINT, INVALID_CODEPOINT, INVALID_CODEPOINT, INVALID_CODEPOINT },
		.exp_error = UTF8_INVALID
	},
	{ // valid 10 bytes followed invalid 2-byte sequence (second byte missing)
		.i = 28,
		.str = (char *)(unsigned char[]) { 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0xC3 },
		.len = 11,
		.exp_len = 10,
		.exp_cp = (uint_least32_t *)(uint_least32_t[]) { 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E },
		.exp_error = UTF8_TOO_SHORT
	}
};

static const struct {
	size_t i;			   // test number
	uint_least32_t *cp;    // input codepoints
	size_t len;			   // number of codepoints
	char *exp_str;         // expected UTF-8 byte sequence
	size_t exp_len;        // expected length of UTF-8 sequence
	Utf8Error exp_error; // expected error
} encoder_test[] = {
	{ // invalid codepoint (UTF-16 surrogate half)
		.i = 1,
		.cp = (uint_least32_t *)(uint_least32_t[]) { 0xD800 },
		.len = 1,
		.exp_str = (char *)(unsigned char[]) { 0xEF, 0xBF, 0xBD },
		.exp_len = 3,
		.exp_error = UTF8_INVALID
	},
	{ // invalid codepoint (UTF-16-unrepresentable)
		.i = 2,
		.cp = (uint_least32_t *)(uint_least32_t[]) { 0x110000 },
		.len = 1,
		.exp_str = (char *)(unsigned char[]) { 0xEF, 0xBF, 0xBD },
		.exp_len = 3,
		.exp_error = UTF8_INVALID
	},
	{ // codepoint encoded to a 1-byte sequence
		.i = 3,
		.cp = (uint_least32_t *)(uint_least32_t[]) { 0x01 },
		.len = 1,
		.exp_str = (char *)(unsigned char[]) { 0x01 },
		.exp_len = 1,
		.exp_error = UTF8_OK
	},
	{ // codepoint encoded to a 2-byte sequence
		.i = 4,
		.cp = (uint_least32_t *)(uint_least32_t[]) { 0xFF },
		.len = 1,
		.exp_str = (char *)(unsigned char[]) { 0xC3, 0xBF },
		.exp_len = 2,
		.exp_error = UTF8_OK
	},
	{ // codepoint encoded to a 3-byte sequence
		.i = 5,
		.cp = (uint_least32_t *)(uint_least32_t[]) { 0xFFF },
		.len = 1,
		.exp_str = (char *)(unsigned char[]) { 0xE0, 0xBF, 0xBF },
		.exp_len = 3,
		.exp_error = UTF8_OK
	},
	{ // codepoint encoded to a 4-byte sequence
		.i = 6,
		.cp = (uint_least32_t *)(uint_least32_t[]) { 0xFFFFF },
		.len = 1,
		.exp_str = (char *)(unsigned char[]) { 0xF3, 0xBF, 0xBF, 0xBF },
		.exp_len = 4,
		.exp_error = UTF8_OK
	},
};

int main(int argc, char *argv[]) {
	size_t i, j, failed, max_cp = 10;

	(void)argc;

	// DECODER
	for (i = 0, failed = 0; i < LEN(decoder_test); i++) {
		size_t len = 0;
		uint_least32_t cp[max_cp];
		for (j = 0; j < max_cp; j++) { cp[j] = 0; }
		Utf8Error error = utf8_decode(decoder_test[i].str, decoder_test[i].len, false, max_cp, cp, &len);

		bool cp_match = true;
		for (j = 0; j < decoder_test[i].exp_len; j++) {
			if (decoder_test[i].exp_cp[j] != cp[j]) { cp_match = false; }
		}

		bool len_match = true;
		size_t measured_len = 0;
		size_t processed_bytes;
		utf8_measure_codepoints(decoder_test[i].str, decoder_test[i].len, &measured_len, &processed_bytes);
		len_match = len == measured_len;

		if (len != decoder_test[i].exp_len) {
			fprintf(stderr, "%s: Failed DECODER test %lu: ", argv[0], decoder_test[i].i);
			fprintf(stderr, "Expected len: %lu, Got: %lu\n", decoder_test[i].exp_len, len);
			failed++;
		}
		if (!cp_match) {
			fprintf(stderr, "%s: Failed DECODER test %lu: codepoints don't match", argv[0], decoder_test[i].i);
			failed++;
		}
		if (error != decoder_test[i].exp_error) {
			fprintf(stderr, "%s: Failed DECODER test %lu: ", argv[0], decoder_test[i].i);
			fprintf(stderr, "Expected error: %u, Got: %u\n", decoder_test[i].exp_error, error);
			failed++;
		}
		if (!len_match) {
			fprintf(stderr, "%s: Failed DECODER test %lu: ", argv[0], decoder_test[i].i);
			fprintf(stderr, "Expected measured len: %lu, Got: %lu\n", len, measured_len);
			failed++;
		}
	}

	// ENCODER

	size_t max_chars = 10;
	for (i = 0, failed = 0; i < LEN(encoder_test); i++) {
		size_t len = 0;
		char str[max_chars];
		for (j = 0; j < max_chars; j++) { str[j] = 'u'; }
		Utf8Error error = utf8_encode(encoder_test[i].cp, encoder_test[i].len, false, encoder_test[i].exp_len + 1, str, &len);

		bool str_match = true;
		for (j = 0; j < encoder_test[i].exp_len; j++) {
			if (encoder_test[i].exp_str[j] != str[j]) { str_match = false; }
		}

		bool len_match = true;
		size_t measured_len = 0;
		Utf8Error measured_error = utf8_measure_bytes(encoder_test[i].cp, encoder_test[i].len, false, &measured_len);
		len_match = len == measured_len;

		if (len != encoder_test[i].exp_len) {
			fprintf(stderr, "%s: Failed ENCODER test %lu: ", argv[0], encoder_test[i].i);
			fprintf(stderr, "Expected len: %lu, Got: %lu\n", encoder_test[i].exp_len, len);
			failed++;
		}
		if (!str_match) {
			fprintf(stderr, "%s: Failed ENCODER test %lu: ", argv[0], encoder_test[i].i);
			fprintf(stderr, "Expected string: %s, Got: %s\n", encoder_test[i].exp_str, str);
			failed++;
		}
		if (error != encoder_test[i].exp_error) {
			fprintf(stderr, "%s: Failed ENCODER test %lu: ", argv[0], encoder_test[i].i);
			fprintf(stderr, "Expected error: %u, Got: %u\n", encoder_test[i].exp_error, error);
			failed++;
		}
		if (!len_match) {
			fprintf(stderr, "%s: Failed ENCODER test %lu: ", argv[0], encoder_test[i].i);
			fprintf(stderr, "Expected measured len: %lu, Got: %lu\n", len, measured_len);
			failed++;
		}
		if (!len_match) {
			fprintf(stderr, "%s: Failed ENCODER test %lu: ", argv[0], encoder_test[i].i);
			fprintf(stderr, "Expected measured error len: %u, Got: %u\n", encoder_test[i].exp_error, measured_error);
			failed++;
		}
		if (str[encoder_test[i].exp_len] != '\0') {
			fprintf(stderr, "%s: Failed ENCODER test %lu: ", argv[0], encoder_test[i].i);
			fprintf(stderr, "String not null terminated at expected len\n");
			failed++;
		}
	}
	return (failed > 0) ? 1 : 0;
}