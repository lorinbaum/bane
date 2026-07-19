#ifndef BANE_H
#define BANE_H

#include <stdlib.h>
#include <stdint.h>
#include "raylib.h"
#include <assert.h>
#include <string.h>



typedef struct {
	uint32_t key; // 32 instead of 16 for easiser key generation: character keycode can be used in full
	uint16_t x, y, w, h;
    int16_t origin_x, origin_y; // Used to offset position of rendered texture.
} TextureRect;

#define RECTMAP_MAX_LOADFACTOR 0.6
typedef enum { RM_OK = 0, RM_MAX_SIZE_REACHED = 1, RM_KEY_NOT_FOUND = 2 } RmError;

// Open addressing hashmap with UINT16_MAX - 1 maximum entries
typedef struct {
    uint16_t max_entries;
    uint16_t used;
    TextureRect *entries;
    uint32_t bucket_count;
    uint16_t *buckets;
} RectMap;

RmError rectmap_create(uint16_t max_entries, RectMap *ret) __nonnull((2));
RmError rectmap_get(RectMap map, uint32_t key, TextureRect *ret) __nonnull((3));
RmError rectmap_put(RectMap *map, TextureRect rect) __nonnull((1));
void rectmap_destroy(RectMap *map) __nonnull((1));

// Indirection required to use __COUNTER__ and similar as argument.
// Inspiration from linux kernel include/linux/minmax.h
#define ___JOIN(a, b) a##b
#define __JOIN(a, b) ___JOIN(a, b)
#define __UNIQUE __JOIN(_unique_, __COUNTER__)

// STRETCHY BUFFERS

#define sb_declare(type) typedef struct { type *items; size_t count, cap; } __JOIN(sb_, type)

#define _sb_create(type, sb, cap_v, cap_k) __extension__({\
    size_t cap_k = (cap_v); \
    __JOIN(sb_, type) sb = { .items = malloc(cap_k * sizeof(type)), .count = 0, .cap = cap_k };\
    ensure(sb.items != NULL);\
    sb;})
#define sb_create(type, cap) _sb_create(type, __UNIQUE, cap, __UNIQUE)

#define _sb_destroy(sb_ptr, sb) __extension__({\
    assert(sb_ptr != NULL);\
    __auto_type sb = (sb_ptr);\
    free(sb->items);\
    sb->items = NULL;\
    sb->count = 0;\
    sb->cap = 0;})
#define sb_destroy(sb_ptr) _sb_destroy(sb_ptr, __UNIQUE)

#define _sb_get(sb_v, sb, index_v, index) __extension__({\
    __auto_type sb = (sb_v); size_t index = (index_v);\
    assert(index < sb.count);\
    sb.items[index];})
#define sb_get(sb, index) _sb_get(sb, __UNIQUE, index, __UNIQUE)

#define _sb_rget(sb_v, sb, index_v, index) __extension__({\
    __auto_type sb = (sb_v); size_t index = (index_v);\
    assert(index <= sb.count && index > 0);\
    sb.items[sb.count - index];})
#define sb_rget(sb, index) _sb_rget(sb, __UNIQUE, index, __UNIQUE)

#define _sb_set(sb_v, sb, index_v, index, value, ...) __extension__({\
    __auto_type sb = (sb_v); size_t index = (index_v); __typeof__(*sb.items) value = (__VA_ARGS__);\
    assert(index < sb.count);\
    sb.items[index] = value;})
#define sb_set(sb, index, ...) _sb_set(sb, __UNIQUE, index, __UNIQUE, __UNIQUE, __VA_ARGS__)

#define _sb_rset(sb_v, sb, index_v, index, value, ...) __extension__({\
    __auto_type sb = (sb_v); size_t index = (index_v); __typeof__(sb.items[0]) value = (__VA_ARGS__);\
    assert(index <= sb.count && index > 0);\
    sb.items[sb.count - index] = value;})
#define sb_rset(sb, index, ...) _sb_rset(sb, __UNIQUE, index, __UNIQUE, __UNIQUE, __VA_ARGS__)

#define _sb_rset_attr(sb_v, sb, index_v, index, attr, value, ...) __extension__({\
    __auto_type sb = (sb_v); size_t index = (index_v); __typeof__(sb.items[0].attr) value = (__VA_ARGS__);\
    assert(index <= sb.count && index > 0);\
    sb.items[sb.count - index].attr = value;})
#define sb_rset_attr(sb, index, attr, ...) _sb_rset_attr(sb, __UNIQUE, index, __UNIQUE, attr, __UNIQUE, __VA_ARGS__)

#define __sb_expand(sb_ptr, sb) __extension__({\
    assert(sb_ptr != NULL);\
    __auto_type sb = (sb_ptr);\
    if (sb->count >= sb->cap) { \
        sb->cap = sb->count > 0 ? sb->count * 2 : 16; \
        sb->items = realloc(sb->items, sizeof(sb->items[0]) * sb->cap); \
        ensure(sb->items != NULL); \
    }})
#define _sb_expand(sb_ptr) __sb_expand(sb_ptr, __UNIQUE)

#define _sb_append(sb_ptr, sb, value, ...) __extension__({\
    assert(sb_ptr != NULL);\
    __auto_type sb = (sb_ptr); __typeof__(sb->items[0]) value = (__VA_ARGS__);\
    _sb_expand(sb);\
    sb->items[sb->count] = value;\
    sb->count++;})
#define sb_append(sb_ptr, ...) _sb_append(sb_ptr, __UNIQUE, __UNIQUE, __VA_ARGS__)

#define _sb_insert(sb_ptr, sb, index_v, index, value, ...) __extension__({\
    assert(sb_ptr != NULL);\
    __auto_type sb = (sb_ptr); size_t index = (index_v); __typeof__(sb->items[0]) value = (__VA_ARGS__);\
    assert(index <= sb->count);\
    _sb_expand(sb);\
    memmove(sb->items + index + 1, sb->items + index, sizeof(sb->items[0]) * (sb->count - index));\
    sb->items[index] = value;\
    sb->count++;})
#define sb_insert(sb_ptr, index, ...) _sb_insert(sb_ptr, __UNIQUE, index, __UNIQUE, __UNIQUE, __VA_ARGS__)

#define _sb_delete(sb_ptr, sb, index_v, index) __extension__({\
    assert(sb_ptr != NULL);\
    __auto_type sb = (sb_ptr); size_t index = (index_v);\
    assert(index < sb->count);\
    memmove(sb->items + index, sb->items + index + 1, sizeof(sb->items[0]) * (sb->count - index - 1));\
    sb->count--;})
#define sb_delete(sb_ptr, index) _sb_delete(sb_ptr, __UNIQUE, index, __UNIQUE)

#define _sb_copy(dest_ptr, dest, src_ptr, src) __extension__({\
    assert(dest_ptr != NULL && src_ptr != NULL);\
    __auto_type dest = (dest_ptr); __auto_type src = (src_ptr);\
    assert(dest->items != src->items);\
    if (src->cap > dest->cap) {\
        free(dest->items);\
        dest->items = malloc(sizeof(src->items[0]) * src->cap);\
        ensure(dest->items != NULL);\
        dest->cap = src->cap;\
    }\
    dest->count = src->count;\
    memcpy(dest->items, src->items, sizeof(src->items[0]) * src->count);})
#define sb_copy(sb_dest_ptr, sb_src_ptr) _sb_copy(sb_dest_ptr, __UNIQUE, sb_src_ptr, __UNIQUE)

// MIN MAX

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

void ensure_fail(const char *file, int line, const char *func, const char *expr) __nonnull((1,3,4));
#define ensure(expr) ((expr) ? (void) (0) : ensure_fail(__FILE__, __LINE__, __func__, #expr))

typedef struct {
    int x;
    int y;
} IntVec2;

sb_declare(IntVec2);
sb_declare(int_least32_t);

#define DEFAULT_TEXTURE_ATLAS_SIZE 64
#define DEFAULT_SKYLINE_ANCHOR_CAP 32
#define DEFAULT_TEXTURE_RECTS_CAP 64

typedef struct {
    int channels;
    int size; // texture area is square of side length "size".
    int max_size; // hardware constrained.
    sb_IntVec2 skyline_anchors;
    RectMap rects;
    Image image; // Holds texture on CPU.
    Texture2D texture; // raylib texture on GPU. Used for drawing.
    bool image_changed;
} TextureAtlas;

typedef enum { TA_OK = 0, TA_MAX_SIZE_EXCEEDED = 1, TA_RECT_NOT_FOUND = 2 } TaError;

// square packing area starts out small, then expands until it reaches a side length of max_size
TextureAtlas* texture_atlas_create(int max_size); 
void texture_atlas_destroy(TextureAtlas **texture_atlas) __nonnull((1));

// Add a new rect to your texture atlas or returns existing rect if key exists already.
TaError texture_atlas_add_get_rect(TextureAtlas *texture_atlas, uint32_t key, Image image, int origin_x, int origin_y, TextureRect *return_rect) __nonnull((1,6));
TaError texture_atlas_get_rect(TextureAtlas *texture_atlas, uint32_t key, TextureRect *return_rect) __nonnull((1,3));

void texture_atlas_update_texture(TextureAtlas *texture_atlas) __nonnull((1));
TaError texture_atlas_draw(TextureAtlas *texture_atlas, uint32_t key, int x, int y, Color tint) __nonnull((1));

// UTF8

typedef enum { UTF8_OK = 0, UTF8_TOO_SHORT = 1, UTF8_INVALID = 2, UTF8_INVALID_ARGUMENT = 3} Utf8Error;

/**
 * @brief Decode UTF8 string into unicode codepoints.
 * @param str UTF8 encoded string to decode.
 * @param in_len Maximum bytes of str to consider.
* @param strict Behavior on invalid encoding: 
 *     true: return UTF8_INVALID immediately.
 *     false: decode as U+FFFD (Replacement Character) and continue.
 * @param out_cap Maximum number codepoints to decode.
 * @param codepoints Decoded codepoints. Written on the go until error or done.
 * @param out_len Number of codepoints decoded. Written on the go until error or done. Can be NULL.
 * @retval UTF8_OK Success.
 * @retval UTF8_TOO_SHORT in_len cuts input mid-utf8-sequence (takes priority over UTF8_INVALID if both apply).
 * @retval UTF8_INVALID Some part of the input is invalid, including if strict is false.
 */
Utf8Error utf8_decode(const char *str, size_t in_len, bool strict, size_t out_cap, uint32_t *codepoints, size_t *out_len) __nonnull((1,5));

/**
 * @brief Encode unicode codepoints as UTF8 string.
 * @param codepoints Codepoints to encode.
 * @param in_len Maximum number of codepoints to encode
 * @param strict Behavior on invalid codepoints: 
 *     true: return UTF8_INVALID immediately.
 *     false: encode as U+FFFD (Replacement Character) and continue.
 * @param out_cap Maximum number bytes to write. If reached while decoding, stops without error
 * @param str Encoded string. Written on the go until error or done.
 * @param out_len Number of bytes encoded. Written on the go until error or done. Can be NULL.
 * @retval UTF8_OK Success.
 * @retval UTF8_INVALID_ARGUMENT NULL out_cap is 0. needs at least one for '\0'.
 * @retval UTF8_TOO_SHORT out_cap cuts output mid-utf8-sequence (takes priority over UTF8_INVALID if both apply).
 * @retval UTF8_INVALID Some part of the input is invalid, including if strict is false.
 */
Utf8Error utf8_encode(const uint32_t *codepoints, size_t in_len, bool strict, size_t out_cap, char *str, size_t *out_len) __nonnull((1,5));

/**
 * @brief Validate codepoints and count number of bytes necessary to encode them.
 * @param codepoints Codepoints to encode.
 * @param in_len Maximum number of codepoints to encode.
 * @param strict Behavior on invalid codepoints: 
 *     true: return UTF8_INVALID immediately.
 *     false: encode as U+FFFD (Replacement Character) and continue.
 * @param out_len Number of bytes needed. Written on the go until error or done.
 * @retval UTF8_OK Success.
 * @retval UTF8_INVALID Some part of the input is invalid, including if strict is false.
 */
Utf8Error utf8_measure_bytes(const uint32_t *codepoints, size_t in_len, bool strict, size_t *out_len) __nonnull((1,4));

/**
 * @brief Count number of codepoints encoded in string. Does not validate input.
 * @param str UTF8 encoded string to decode.
 * @param in_len Maximum number of bytes to decode.
 * @param out_len Number of codepoints in str. Written on the go until error or done.
 * @param processed_bytes Number of input bytes processed. If UTF8_TOO_SHORT, this does not include incomplete sequences. Can be NULL.
 * @retval UTF8_OK Success.
 * @retval UTF8_TOO_SHORT in_len cuts input mid-utf8-sequence
 */
Utf8Error utf8_measure_codepoints(const char *str, size_t in_len, size_t *out_len, size_t *processed_bytes) __nonnull((1,3));

#define INVALID_CODEPOINT UINT32_C(0xFFFD)

// TEXT

#define SCALE 1.25 // HACK: known scaling on testing machine

#define LF 0xA // \n
#define SPACE 0x20

#include "ft2build.h"
#include FT_FREETYPE_H

typedef struct {
    size_t cap, gap, offset;
    uint32_t *data;
} GapBuffer;

GapBuffer gb_create_from_text(char *text, size_t str_len);
GapBuffer gb_create(size_t cap);
void gb_destroy(GapBuffer *gb) __nonnull((1));
char *gb_encode(GapBuffer gb, size_t offset, size_t length);
size_t gb_count(GapBuffer gb);
void gb_insert(GapBuffer *gb, size_t index, uint32_t value) __nonnull((1));
void gb_insert_n(GapBuffer *gb, size_t index, uint32_t *values, size_t n) __nonnull((1,3));
void gb_delete_n(GapBuffer *gb, size_t index, size_t n) __nonnull((1));
uint32_t gb_get(GapBuffer gb, size_t index);

typedef struct {
    int_least32_t x, y, w, h, scroll_y; // scroll_y gets negative when scrolling down
    GapBuffer gb;
} TextBox;

typedef struct {
    FT_Face face;
    uint_least32_t line_height;
    // NOTE: text_selected_color is near-useless until blending is added and selection draws behind text.
    Color text_color, text_selected_color, cursor_color, selection_color;
} Style;

typedef struct {
    size_t offset;
    bool pre_wrap;          // wrapping lines have two valid position for each offset. This control whether to render before or after wrap
    int_least32_t sticky_x; // x coordinate that persists across vertical movement
} CursorLoc;

typedef struct {
    CursorLoc loc;
    CursorLoc sel_start;    // location where the selection - if any - started while loc is the cursor location (may be before or after sel_start)
    bool sel_active;        // if false, sel_start is irrelevant
    bool update_sticky_x;   // set to true by movement functions that change sticky_x. sticky_x is not continuously updated, only when needed
    bool scroll_to;         // set to true to scroll to put cursor into view in next frame
} Cursor;

void draw_text(TextBox *box, Style style, TextureAtlas *atlas, Cursor *cursor) __nonnull((1,3,4));

void cursor_right(TextBox box, Cursor *cursor, bool selecting) __nonnull((2));
void cursor_left(Cursor *cursor, bool selecting) __nonnull((1));
void cursor_down(TextBox box, Style style, Cursor *cursor, bool selecting) __nonnull((3));
void cursor_up(TextBox box, Style style, Cursor *cursor, bool selecting) __nonnull((3));
void cursor_home(TextBox box, Style style, Cursor *cursor, bool selecting) __nonnull((3));
void cursor_end(TextBox box, Style style, Cursor *cursor, bool selecting) __nonnull((3));
void cursor_mouse(TextBox box, Style style, Cursor *cursor, int_least32_t x, int_least32_t y, bool selecting) __nonnull((3));
void cursor_page_up(TextBox box, Style style, Cursor *cursor, bool selecting) __nonnull((3));
void cursor_page_down(TextBox box, Style style, Cursor *cursor, bool selecting)  __nonnull((3));
void cursor_next_word(TextBox box, Cursor *cursor, bool selecting) __nonnull((2));
void cursor_prev_word(TextBox box, Cursor *cursor, bool selecting) __nonnull((2));

void cursor_write(TextBox *box, Cursor *cursor, uint32_t c) __nonnull((1,2));
void cursor_backspace(TextBox *box, Cursor *cursor) __nonnull((1,2));
void cursor_delete(TextBox *box, Cursor *cursor) __nonnull((1,2));

void cursor_copy(TextBox box, Cursor cursor);
void cursor_paste(TextBox *box, Cursor *cursor) __nonnull((1,2));

#endif
