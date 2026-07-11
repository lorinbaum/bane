#ifndef BANE_H
#define BANE_H

#include <stdlib.h>
#include <stdint.h>
#include "raylib.h"
#include <assert.h>

typedef struct {
	uint32_t key; // 32 instead of 16 for easiser key generation: character keycode can be used in full
	uint16_t x, y, w, h;
    int16_t origin_x, origin_y; // Used to offset position of rendered texture.
} TextureRect;

#define RECTMAP_MAX_LOADFACTOR 0.6
typedef enum { RM_OK = 0, RM_MAX_SIZE_REACHED = 1, RM_KEY_NOT_FOUND = 2 } RMStatus;

// Open addressing hashmap with UINT16_MAX - 1 maximum entries
typedef struct {
    uint16_t max_entries;
    uint16_t used;
    TextureRect *entries;
    uint32_t bucket_count;
    uint16_t *buckets;
} RectMap;

RMStatus rectmap_create(RectMap *ret, uint16_t max_entries);
RMStatus rectmap_get(TextureRect *ret, RectMap map, uint32_t key);
RMStatus rectmap_put(RectMap *map, TextureRect rect);
void rectmap_destroy(RectMap *map);

// requires GCC or CLANG because __typeof__ not in std=c17
#define TYPEOF __typeof__

#define ARRAY_DECLARE(STRUCT) \
    typedef struct {\
        STRUCT *items; \
        size_t count, cap; \
    } STRUCT##Array; \
    STRUCT##Array create_##STRUCT##Array(size_t cap); \
    void destroy_##STRUCT##Array(STRUCT##Array *a); \
    void expand_##STRUCT##Array(STRUCT##Array *a); \
    void append_##STRUCT(STRUCT##Array *a, TYPEOF(*a->items) item); \
    void delete_##STRUCT(STRUCT##Array *a, size_t index); \
    void insert_##STRUCT(STRUCT##Array *a, size_t index, TYPEOF(*a->items) item);

#define ARRAY_DEFINE(STRUCT) \
    STRUCT##Array create_##STRUCT##Array(size_t cap) { \
        STRUCT##Array ret = {malloc(cap * sizeof(STRUCT)), 0, cap}; \
        ensure(ret.items != NULL); \
        return ret; \
    } \
    void destroy_##STRUCT##Array(STRUCT##Array *a) { \
        free(a->items); \
        a->items = NULL; \
        a->count = 0; \
        a->cap = 0; \
    } \
    void expand_##STRUCT##Array(STRUCT##Array *a) { \
        if (a->count >= a->cap) { \
            a->cap = max(a->count * 2, a->cap > 0 ? a->cap * 2 : 16); \
            a->items = realloc(a->items, sizeof(a->items[0]) * a->cap); \
            ensure(a->items != NULL); \
        }\
    } \
    void append_##STRUCT(STRUCT##Array *a, TYPEOF(*a->items) item) { \
        expand_##STRUCT##Array(a); \
        a->items[a->count] = item; \
        a->count++; \
    } \
    void delete_##STRUCT(STRUCT##Array *a, size_t index) { \
        assert(index < a->count); \
        for (size_t i = index; i + 1 < a->count; i++) { a->items[i] = a->items[i+1]; } \
        a->count--; \
    } \
    void insert_##STRUCT(STRUCT##Array *a, size_t index, TYPEOF(*a->items) item) { \
        assert(index <= a->count); \
        expand_##STRUCT##Array(a); \
        for (size_t i = a->count; i > index; i--) { a->items[i] = a->items[i-1]; } \
        a->items[index] = item; \
        a->count++; \
    }

// Indirection required to use __COUNTER__ and similar as argument.
// Inspiration from linux kernel include/linux/minmax.h
#define ___JOIN(a, b) a##b
#define __JOIN(a, b) ___JOIN(a, b)
#define __UNIQUE __JOIN(_unique_, __COUNTER__)

#define __min(x, y, x0, y0) __extension__({ \
    __auto_type x0 = (x); \
    __auto_type y0 = (y); \
    x0 > y0 ? y0 : x0; \
})

#define min(x, y) __min(x, y, __UNIQUE, __UNIQUE)

#define __max(x, y, x0, y0) __extension__({ \
    __auto_type x0 = (x); \
    __auto_type y0 = (y); \
    x0 > y0 ? x0 : y0; \
})

#define max(x, y) __max(x, y, __UNIQUE, __UNIQUE)

#define __between(x, lower, upper, x0, lower0, upper0) __extension__({ \
    __auto_type x0 = (x); \
    __auto_type lower0 = (lower); \
    __auto_type upper0 = (upper); \
    lower0 <= x0 && x0 <= upper0; \
})

#define between(x, lower, upper) __between(x, lower, upper, __UNIQUE, __UNIQUE, __UNIQUE)

void ensure_fail(const char *file, int line, const char *func, const char *expr);
#define ensure(expr) ((expr) ? (void) (0) : ensure_fail(__FILE__, __LINE__, __func__, #expr))

typedef struct {
    int x;
    int y;
} IntVec2;

ARRAY_DECLARE(IntVec2)

#define DEFAULT_TEXTURE_ATLAS_SIZE 64
#define DEFAULT_SKYLINE_ANCHOR_CAP 32
#define DEFAULT_TEXTURE_RECTS_CAP 64

typedef struct {
    int channels;
    int size; // texture area is square of side length "size".
    int max_size; // hardware constrained.
    IntVec2Array skyline_anchors;
    RectMap rects;
    Image image; // Holds texture on CPU.
    Texture2D texture; // raylib texture on GPU. Used for drawing.
    bool image_changed;
} TextureAtlas;

typedef enum { TA_OK = 0, TA_MAX_SIZE_EXCEEDED = 1, TA_RECT_NOT_FOUND = 2 } TAStatus;

// square packing area starts out small, then expands until it reaches a side length of max_size
TextureAtlas* texture_atlas_create(int max_size); 
void texture_atlas_destroy(TextureAtlas **texture_atlas);

// Add a new rect to your texture atlas or returns existing rect if key exists already.
TAStatus texture_atlas_add_get_rect(TextureRect *return_rect, TextureAtlas *texture_atlas, uint32_t key, Image texture, int origin_x, int origin_y);
TAStatus texture_atlas_get_rect(TextureRect *return_rect, TextureAtlas *texture_atlas, uint32_t key);

void texture_atlas_update_texture(TextureAtlas *texture_atlas);
TAStatus texture_atlas_draw(TextureAtlas *texture_atlas, uint32_t key, int x, int y, Color tint);

// UTF8

typedef enum { UTF8_OK = 0, UTF8_TOO_SHORT = 1, UTF8_INVALID = 2, UTF8_INVALID_ARGUMENT = 3} UTF8Status;

/**
 * @brief Decode UTF8 string into unicode codepoints.
 * @param str UTF8 encoded string to decode.
 * @param in_len Maximum bytes of str to consider.
* @param strict Behavior on invalid encoding: 
 *     true: return UTF8_INVALID immediately.
 *     false: decode as U+FFFD (Replacement Character) and continue.
 * @param out_cap Maximum number codepoints to decode.
 * @param codepoints Decoded codepoints. Written on the go until error or done.
 * @param out_len Number of codepoints decoded. Written on the go until error or done.
 * @retval UTF8_OK Success.
 * @retval UTF8_INVALID_ARGUMENT NULL is passed, nothing happened.
 * @retval UTF8_TOO_SHORT in_len cuts input mid-utf8-sequence (takes priority over UTF8_INVALID if both apply).
 * @retval UTF8_INVALID Some part of the input is invalid, including if strict is false.
 */
UTF8Status utf8_decode(const char *str, size_t in_len, bool strict, size_t out_cap, uint32_t *codepoints, size_t *out_len);

/**
 * @brief Encode unicode codepoints as UTF8 string.
 * @param codepoints Codepoints to encode.
 * @param in_len Maximum number of codepoints to encode
 * @param strict Behavior on invalid codepoints: 
 *     true: return UTF8_INVALID immediately.
 *     false: encode as U+FFFD (Replacement Character) and continue.
 * @param out_cap Maximum number bytes to write. If reached while decoding, stops without error
 * @param str Encoded string. Written on the go until error or done.
 * @param out_len Number of bytes encoded. Written on the go until error or done.
 * @retval UTF8_OK Success.
 * @retval UTF8_INVALID_ARGUMENT NULL is passed, nothing happened.
 * @retval UTF8_TOO_SHORT out_cap cuts output mid-utf8-sequence (takes priority over UTF8_INVALID if both apply).
 * @retval UTF8_INVALID Some part of the input is invalid, including if strict is false.
 */
UTF8Status utf8_encode(const uint32_t *codepoints, size_t in_len, bool strict, size_t out_cap, char *str, size_t *out_len);

/**
 * @brief Validate codepoints and count number of bytes necessary to encode them.
 * @param codepoints Codepoints to encode.
 * @param in_len Maximum number of codepoints to encode.
 * @param strict Behavior on invalid codepoints: 
 *     true: return UTF8_INVALID immediately.
 *     false: encode as U+FFFD (Replacement Character) and continue.
 * @param out_len Number of bytes needed. Written on the go until error or done.
 * @retval UTF8_OK Success.
 * @retval UTF8_INVALID_ARGUMENT NULL is passed, nothing happened.
 * @retval UTF8_INVALID Some part of the input is invalid, including if strict is false.
 */
UTF8Status utf8_measure_bytes(const uint32_t *codepoints, size_t in_len, bool strict, size_t *out_len);

/**
 * @brief Count number of codepoints encoded in string. Does not validate input.
 * @param str UTF8 encoded string to decode.
 * @param in_len Maximum number of bytes to decode.
 * @param out_len Number of codepoints in str. Written on the go until error or done.
 * @param processed_bytes Number of input bytes processed. If UTF8_TOO_SHORT, this does not include incomplete sequences.
 * @retval UTF8_OK Success.
 * @retval UTF8_INVALID_ARGUMENT NULL is passed, nothing happened.
 * @retval UTF8_TOO_SHORT in_len cuts input mid-utf8-sequence
 */
UTF8Status utf8_measure_codepoints(const char *str, size_t in_len, size_t *out_len, size_t *processed_bytes);

#define INVALID_CODEPOINT UINT32_C(0xFFFD)

#endif
