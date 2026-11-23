#ifndef BANE_H
#define BANE_H

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include "SDL3/SDL.h"

// Indirection required to use __COUNTER__ and similar as argument.
// Inspiration from linux kernel include/linux/minmax.h
#define ___JOIN(a, b) a##b
#define __JOIN(a, b) ___JOIN(a, b)
#define __UNIQUE __JOIN(_unique_, __COUNTER__)

// HELPERS

typedef struct Rectangle { int_least32_t x, y, width, height; } Rectangle;
/**
 * Transform fit to fit box.
 * if linked_fit is not NULL, transformations applied to fit also apply to linked_fit, but linked_fit isn't evaluated to fit box
 */
void rect_fit_box(Rectangle box, Rectangle *fit, Rectangle *linked_fit) __nonnull((2));

// stretchy buffers

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

// IMAGE

// 0-255
typedef struct Color { float r, g, b, a; } Color;
bool color_is_valid(Color c);

typedef enum ImageFormat { IMAGE_FORMAT_ALPHA, IMAGE_FORMAT_RGB } ImageFormat;
uint8_t get_pixel_data_size(ImageFormat format);

typedef struct Image { uint8_t *data; size_t width, height; ImageFormat format; } Image;
Image image_create(size_t width, size_t height, ImageFormat format);
/**
 * Create image pointing to data. Does not make a copy.
 */
Image image_create_from_data(uint8_t *data, size_t width, size_t height, ImageFormat format) __nonnull((1));
void image_destroy(Image *img) __nonnull((1));
/**
 * Draw tinted src at dest.
 * If src is IMAGE_FORMAT_ALPHA, applies tint at opacity of src pixel
 * if dest is IMAGE_FORMAT_ALPHA, tint is converted to average gray of r,g,b channels.
 * Not yet supported:
 * - from IMAGE_FORMAT_ALPHA to IMAGE_FORMAT_RGB
 * - Scaling. dest_rect and src_rect must have same dimensions
 */
void image_draw_tint(Image dest, Image src, Rectangle dest_rect, Rectangle src_rect, Color tint);

/**
 * Fill a rect in dest with color. If dest is IMAGE_FORMAT_ALPHA, applies average of color's r,g,b channels.
 */
void image_draw_rect(Image dest, Rectangle rect, Color color);

/**
 * Create a new image of w x h and draw image in it at x, y. Fill new background with color.
 */
Image image_resize(Image image, int_least32_t w, int_least32_t h, int_least32_t x, int_least32_t y, Color color); 

/**
 * Creates a copy of src at dest.
 * Callers responsibility to free dest.data before this!
 * If dest points to src, does nothing.
 */
void image_copy(Image *dest, Image src) __nonnull((1));

// TEXTURE ATLAS

typedef struct TextureRect {
	uint32_t key; // 32 instead of 16 for easiser key generation: character keycode can be used in full
	uint16_t x, y, w, h;
    int16_t origin_x, origin_y; // Used to offset position of rendered texture.
} TextureRect;

#define RECTMAP_MAX_LOADFACTOR 0.6
typedef enum RmError { RM_OK = 0, RM_MAX_SIZE_REACHED = 1, RM_KEY_NOT_FOUND = 2 } RmError;

// Open addressing hashmap with UINT16_MAX - 1 maximum entries
typedef struct RectMap {
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

#define DEFAULT_TEXTURE_ATLAS_SIZE 64
#define DEFAULT_SKYLINE_ANCHOR_CAP 32
#define DEFAULT_TEXTURE_RECTS_CAP 64

typedef struct IntVec2 {
    int x;
    int y;
} IntVec2;

sb_declare(IntVec2);

typedef struct TextureAtlas {
    size_t size; // texture area is square of side length "size".
    size_t max_size; // hardware constrained.
    sb_IntVec2 skyline_anchors;
    RectMap rects;
    Image image; // Holds texture on CPU.
    bool image_changed;
} TextureAtlas;

typedef enum { TA_OK = 0, TA_MAX_SIZE_EXCEEDED = 1, TA_RECT_NOT_FOUND = 2 } TaError;

// square packing area starts out small, then expands until it reaches a side length of max_size
TextureAtlas texture_atlas_create(int max_size); 
void texture_atlas_destroy(TextureAtlas *texture_atlas) __nonnull((1));

// Adds a new rect or returns existing rect if key exists already.
TaError texture_atlas_add_get_rect(TextureAtlas *texture_atlas, uint32_t key, Image image, int origin_x, int origin_y, TextureRect *return_rect) __nonnull((1,6));
TaError texture_atlas_get_rect(TextureAtlas texture_atlas, uint32_t key, TextureRect *return_rect) __nonnull((3));

TaError texture_atlas_draw(Image dest, Rectangle text_rect, TextureAtlas *texture_atlas, uint32_t key, int x, int y, Color tint) __nonnull((3));

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

// Adaptation from Sean Barrett's https://github.com/nothings/stb/blob/master/stb_truetype.h

#include <stdio.h>

// Determine the number of fonts in a font file.
// TrueType collection (.ttc) files may contain multiple fonts, while TrueType font
// (.ttf) files only contain one font.
// The number of fonts can be used for indexing where the index is between zero and total fonts - 1
// Returns -1 on failure.
int font_count(const unsigned char *data) __nonnull((1));

// Get the font offset for a given index; 
// It returns -1 if the index is out of range.
// A .ttf file will only define one font and it will always be at offset 0.
int font_offset_for_index(const unsigned char *data, int index) __nonnull((1));

typedef struct fontinfo fontinfo;

// Given an offset into the file that defines a font, this function populates fontinfo.
// Returns NULL on failure.
fontinfo *font_load(const unsigned char *data, int offset) __nonnull((1));

// Free fontinfo pointer
void font_destroy(fontinfo *info) __nonnull((1));

// Returns glyph index corresponding to unicode codepoint.
// Returns 0 if the character codepoint is not defined in the font.
int font_glyph_index(const fontinfo *info, int unicode_codepoint) __nonnull((1));

// computes a scale factor to produce a font that has 'pt' size at given screen points per inch. Emulates print behavior.
// It represents pixels per font-unit
float font_scale(const fontinfo *info, float pt, float ppi) __nonnull((1));

// Font metrics in unscaled coordinates:
// - ascent: the coordinate above the baseline the font extends
// - descent: coordinate below the baseline the font extends (i.e. it is typically negative)
// - lineGap: spacing between one row's descent and the next row's ascent
// these are expressed in unscaled coordinates, so you must multiply by
// the scale factor for a given size
void font_vmetrics(const fontinfo *info, int *ascent, int *descent, int *lineGap) __nonnull((1));

// analogous to font_vmetrics, but returns the "typographic" values from the OS/2
// table (specific to MS/Windows TTF files).
// Returns 1 on success (table present), 0 on failure.
int font_vmetrics_OS2(const fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap) __nonnull((1));

// the bounding box around all possible characters in font-units
void font_bounding_box(const fontinfo *info, int *x0, int *y0, int *x1, int *y1) __nonnull((1, 2, 3, 4, 5));

// Horizontal metrics in unscaled coordinates:
// - leftSideBearing: offset from the current horizontal position to the left edge of the character
// - advanceWidth: offset from the current horizontal position to the next horizontal position
void font_hmetrics(const fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing) __nonnull((1));

// an additional amount to add to the 'advance' value between ch1 and ch2 in font-units
int font_kerning(const fontinfo *info, int glyph1, int glyph2) __nonnull((1));

typedef struct kerningentry {
   int glyph1; // use font_glyph_index
   int glyph2;
   int advance;
} kerningentry;

// Retrieves a complete list of all of the kerning pairs provided by the font
// font_kerning_table never writes more than table_length entries and returns how many entries it did write.
// The table will be sorted by (a.glyph1 == b.glyph1)?(a.glyph2 < b.glyph2):(a.glyph1 < b.glyph1)
int font_kerning_table_len(const fontinfo *info); __nonnull((1))
int font_kerning_table(const fontinfo *info, kerningentry* table, int table_length) __nonnull((1, 2));


//////////////////////////////////////////////////////////////////////////////
//
// BITMAP RENDERING
//

// allocates single-channel 8bpp bitmap and renders the specified character/glyph at the specified scale into it,
// with 'sample_count' samples per pixel.
// scale is in pixels per font-unit.
// Sample count improves coverage approximation in each pixel. Must be greater than 0.
// bitmap: 0 is no coverage (transparent), 255 is fully covered (opaque).
// *width & *height are filled out with the width & height of the bitmap,
// which is stored left-to-right, top-to-bottom.
// Baseline is pixel offst from top of the bitmap to baseline. If shift_y = 0, it's an integer.
// you can specify a subpixel shift for the character
unsigned char *font_bitmap(const fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int sample_count, int *width, int *height, float *baseline) __nonnull((1));

// Get bitmap width height and baseline offset and x_min (leftSideBearing) in font-units.
// shift_y is downwards offset of baseline. If shift_y = 0, the baseline sits on a pixel boundary.
// Baseline is pixels offset from top of the bitmap to baseline. If shift_y = 0, it's an integer.
// you can specify a subpixel shift for the character
void font_bitmap_box(const fontinfo *info, int glyph, float scale_x, float scale_y, float shift_x, float shift_y, int *x_min, int *width, int *height, float *baseline) __nonnull((1, 7, 8, 9, 10));

//////////////////////////////////////////////////////////////////////////////
//
// Finding the right font...
//
// You should really just solve this offline, keep your own tables
// of what font is what, and don't try to get it out of the .ttf file.
// That's because getting it out of the .ttf file is really hard, because
// the names in the file can appear in many possible encodings, in many
// possible languages, and e.g. if you need a case-insensitive comparison,
// the details of that depend on the encoding & language in a complex way
// (actually underspecified in truetype, but also gigantic).
//
// But you can use the provided functions in two possible ways:
//     font_match() will use *case-sensitive* comparisons on
//             unicode-encoded names to try to find the font you want;
//             you can run this before calling font_init()
//
//     font_name() lets you get any of the various strings
//             from the file yourself and do your own comparisons on them.
//             You have to have called font_init() first.


int font_match(const unsigned char *fontdata, const char *name, int flags) __nonnull((1, 2));
// returns the offset (not index) of the font that matches, or -1 if none
//   if you use MACSTYLE_DONTCARE, use a font name like "Arial Bold".
//   if you use any other flag, use a font name like "Arial"; this checks
//     the 'macStyle' header field; i don't know if fonts set this consistently
#define MACSTYLE_DONTCARE     0
#define MACSTYLE_BOLD         1
#define MACSTYLE_ITALIC       2
#define MACSTYLE_UNDERSCORE   4
#define MACSTYLE_NONE         8   // <= not same as 0, this makes us check the bitfield is 0

int utf8_eq_utf16_bigendian(const char *s1, int len1, const char *s2, int len2) __nonnull((1, 3));
// returns 1/0 whether the first string interpreted as utf8 is identical to
// the second string interpreted as big-endian utf16... useful for strings from next func

const char *font_name(const fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID) __nonnull((1, 2));
// returns the string (which may be big-endian double byte, e.g. for unicode)
// and puts the length in bytes in *length.
//
// some of the values for the IDs are below; for more see the truetype spec:
//     http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6name.html
//     http://www.microsoft.com/typography/otspec/name.htm

enum { // platformID
   PLATFORM_ID_UNICODE   =0,
   PLATFORM_ID_MAC       =1,
   PLATFORM_ID_ISO       =2,
   PLATFORM_ID_MICROSOFT =3
};

enum { // encodingID for PLATFORM_ID_UNICODE
   UNICODE_EID_UNICODE_1_0    =0,
   UNICODE_EID_UNICODE_1_1    =1,
   UNICODE_EID_ISO_10646      =2,
   UNICODE_EID_UNICODE_2_0_BMP=3,
   UNICODE_EID_UNICODE_2_0_FULL=4
};

enum { // encodingID for PLATFORM_ID_MICROSOFT
   MS_EID_SYMBOL        =0,
   MS_EID_UNICODE_BMP   =1,
   MS_EID_SHIFTJIS      =2,
   MS_EID_UNICODE_FULL  =10
};

enum { // encodingID for PLATFORM_ID_MAC; same as Script Manager codes
   MAC_EID_ROMAN        =0,   MAC_EID_ARABIC       =4,
   MAC_EID_JAPANESE     =1,   MAC_EID_HEBREW       =5,
   MAC_EID_CHINESE_TRAD =2,   MAC_EID_GREEK        =6,
   MAC_EID_KOREAN       =3,   MAC_EID_RUSSIAN      =7
};

enum { // languageID for PLATFORM_ID_MICROSOFT; same as LCID...
       // problematic because there are e.g. 16 english LCIDs and 16 arabic LCIDs
   MS_LANG_ENGLISH     =0x0409,   MS_LANG_ITALIAN     =0x0410,
   MS_LANG_CHINESE     =0x0804,   MS_LANG_JAPANESE    =0x0411,
   MS_LANG_DUTCH       =0x0413,   MS_LANG_KOREAN      =0x0412,
   MS_LANG_FRENCH      =0x040c,   MS_LANG_RUSSIAN     =0x0419,
   MS_LANG_GERMAN      =0x0407,   MS_LANG_SPANISH     =0x0409,
   MS_LANG_HEBREW      =0x040d,   MS_LANG_SWEDISH     =0x041D
};

enum { // languageID for PLATFORM_ID_MAC
   MAC_LANG_ENGLISH      =0 ,   MAC_LANG_JAPANESE     =11,
   MAC_LANG_ARABIC       =12,   MAC_LANG_KOREAN       =23,
   MAC_LANG_DUTCH        =4 ,   MAC_LANG_RUSSIAN      =32,
   MAC_LANG_FRENCH       =1 ,   MAC_LANG_SPANISH      =6 ,
   MAC_LANG_GERMAN       =2 ,   MAC_LANG_SWEDISH      =5 ,
   MAC_LANG_HEBREW       =10,   MAC_LANG_CHINESE_SIMPLIFIED =33,
   MAC_LANG_ITALIAN      =3 ,   MAC_LANG_CHINESE_TRAD =19
};

#define LF 0xA // \n
#define SPACE 0x20

typedef struct GapBuffer {
    size_t cap, gap, offset;
    uint32_t *data;
} GapBuffer;

GapBuffer gb_create_from_text(const char *text, size_t str_len);
GapBuffer gb_create(size_t cap);
void gb_destroy(GapBuffer *gb) __nonnull((1));
char *gb_encode(GapBuffer gb, size_t offset, size_t length);
size_t gb_count(GapBuffer gb);
void gb_insert(GapBuffer *gb, size_t index, uint32_t value) __nonnull((1));
void gb_insert_n(GapBuffer *gb, size_t index, uint32_t *values, size_t n) __nonnull((1,3));
void gb_delete_n(GapBuffer *gb, size_t index, size_t n) __nonnull((1));
uint32_t gb_get(GapBuffer gb, size_t index);

typedef struct CursorLoc {
    size_t offset;
    bool pre_wrap;          // wrapping lines have two valid position for each offset. This control whether to render before or after wrap
    int_least32_t sticky_x; // x coordinate that persists across vertical movement
} CursorLoc;

typedef struct Cursor {
    CursorLoc loc;
    CursorLoc sel_origin;   // location where the selection - if any - started while loc is the cursor location (may be before or after sel_origin)
    bool sel_active;        // if false, sel_origin is ignored
    bool update_sticky_x;   // set to true by movement functions that change sticky_x. sticky_x is not continuously updated, only when needed
    bool scroll_to;         // set to true to scroll to put cursor into view in next frame
} Cursor;

typedef struct Style {
    uint_least32_t font_size, line_height;
    Color text_color, text_selected_color, cursor_color, selection_color;
    fontinfo *font;
} Style;

typedef struct FrameBuffer {
    uint8_t *data; // may be larger than screen, to avoid frequent reallocations
    // width, height tracked instead of flat size because it scales along with sdl texture, which can't be reshaped
    size_t max_width, max_height;
    Image image; // target to draw to framebuffer. links to FrameBuffer.data
    SDL_Texture *texture;
} FrameBuffer;

typedef struct RenderContext {
    fontinfo *font;
    float font_scale;
    int_least32_t line_height, baseline_offset; // each in px
    Color text_color, text_selected_color, cursor_color, selection_color;
    TextureAtlas atlas;
    FrameBuffer fb;
} RenderContext;

void renderctx_create(RenderContext *ctx) __nonnull((1));
void renderctx_load_style(RenderContext *ctx, Style style, float scale) __nonnull((1));
void renderctx_destroy(RenderContext *ctx) __nonnull((1));

typedef struct TextBox {
    int_least32_t x, y, w, h;
    int_least32_t scroll_y; // Gets negative when scrolling down
    GapBuffer gb;
    Cursor cursor;
} TextBox;

TextBox textbox_create(int_least32_t x, int_least32_t y, int_least32_t w, int_least32_t h, const char* text);
bool tb_hit(TextBox box, int_least32_t x, int_least32_t y);

void tb_draw(TextBox *box, RenderContext *ctx) __nonnull((1,2));

void tb_right(TextBox *box, bool selecting) __nonnull((1));
void tb_right_n(TextBox *box, size_t n, bool selecting) __nonnull((1));
void tb_left(TextBox *box, bool selecting) __nonnull((1));
void tb_down(TextBox *box, RenderContext *ctx, bool selecting) __nonnull((1, 2));
void tb_up(TextBox *box, RenderContext *ctx, bool selecting) __nonnull((1, 2));
void tb_home(TextBox *box, RenderContext *ctx, bool selecting) __nonnull((1, 2));
void tb_end(TextBox *box, RenderContext *ctx, bool selecting) __nonnull((1, 2));
void tb_mouse(TextBox *box, RenderContext *ctx, int_least32_t x, int_least32_t y, bool selecting) __nonnull((1, 2));
void tb_page_up(TextBox *box, RenderContext *ctx, bool selecting) __nonnull((1, 2));
void tb_page_down(TextBox *box, RenderContext *ctx, bool selecting)  __nonnull((1, 2));
void tb_next_word(TextBox *box, bool selecting) __nonnull((1));
void tb_prev_word(TextBox *box, bool selecting) __nonnull((1));

void tb_write(TextBox *box, uint32_t c) __nonnull((1));
void tb_write_n(TextBox *box, uint32_t *c, size_t n) __nonnull((1, 2));
void tb_backspace(TextBox *box) __nonnull((1));
void tb_delete(TextBox *box) __nonnull((1));

void tb_copy(TextBox box);
void tb_paste(TextBox *box) __nonnull((1));
void tb_cut(TextBox *box) __nonnull((1));

void tb_select_all(TextBox *box) __nonnull((1));

typedef struct State {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TextBox *last_hit;
    RenderContext ctx;
    TextBox textbox;
    Style style;
    float scale;
    bool draw;
} State;

#endif
