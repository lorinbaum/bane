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

#include <stdio.h>
//
// USAGE
//
//   Include this file in whatever places need to refer to it. In ONE C/C++
//   file, write:
//      #define STB_TRUETYPE_IMPLEMENTATION
//   before the #include of this file. This expands out the actual
//   implementation into that C/C++ file.
//
//   To make the implementation private to the file that generates the implementation,
//      #define STBTT_STATIC
//
//   Simple 3D API (don't ship this, but it's fine for tools and quick start)
//           stbtt_BakeFontBitmap()               -- bake a font to a bitmap for use as texture
//           stbtt_GetBakedQuad()                 -- compute quad to draw for a given char
//
//   Improved 3D API (more shippable):
//           #include "stb_rect_pack.h"           -- optional, but you really want it
//           stbtt_PackBegin()
//           stbtt_PackSetOversampling()          -- for improved quality on small fonts
//           stbtt_PackFontRanges()               -- pack and renders
//           stbtt_PackEnd()
//           stbtt_GetPackedQuad()
//
//   "Load" a font file from a memory buffer (you have to keep the buffer loaded)
//           stbtt_InitFont()
//           stbtt_GetFontOffsetForIndex()        -- indexing for TTC font collections
//           stbtt_GetNumberOfFonts()             -- number of fonts for TTC font collections
//
//   Render a unicode codepoint to a bitmap
//           stbtt_GetCodepointBitmap()           -- allocates and returns a bitmap
//           stbtt_MakeCodepointBitmap()          -- renders into bitmap you provide
//           stbtt_GetCodepointBitmapBox()        -- how big the bitmap must be
//
//   Character advance/positioning
//           stbtt_GetCodepointHMetrics()
//           stbtt_GetFontVMetrics()
//           stbtt_GetFontVMetricsOS2()
//           stbtt_GetCodepointKernAdvance()
//
// ADDITIONAL DOCUMENTATION
//
//   Immediately after this block comment are a series of sample programs.
//
//   After the sample programs is the "header file" section. This section
//   includes documentation for each API function.
//
//   Some important concepts to understand to use this library:
//
//      Codepoint
//         Characters are defined by unicode codepoints, e.g. 65 is
//         uppercase A, 231 is lowercase c with a cedilla, 0x7e30 is
//         the hiragana for "ma".
//
//      Glyph
//         A visual character shape (every codepoint is rendered as
//         some glyph)
//
//      Glyph index
//         A font-specific integer ID representing a glyph
//
//      Baseline
//         Glyph shapes are defined relative to a baseline, which is the
//         bottom of uppercase characters. Characters extend both above
//         and below the baseline.
//
//      Current Point
//         As you draw text to the screen, you keep track of a "current point"
//         which is the origin of each character. The current point's vertical
//         position is the baseline. Even "baked fonts" use this model.
//
//      Vertical Font Metrics
//         The vertical qualities of the font, used to vertically position
//         and space the characters. See docs for stbtt_GetFontVMetrics.
//
//      Font Size in Pixels or Points
//         The preferred interface for specifying font sizes in stb_truetype
//         is to specify how tall the font's vertical extent should be in pixels.
//         If that sounds good enough, skip the next paragraph.
//
//         Most font APIs instead use "points", which are a common typographic
//         measurement for describing font size, defined as 72 points per inch.
//         stb_truetype provides a point API for compatibility. However, true
//         "per inch" conventions don't make much sense on computer displays
//         since different monitors have different number of pixels per
//         inch. For example, Windows traditionally uses a convention that
//         there are 96 pixels per inch, thus making 'inch' measurements have
//         nothing to do with inches, and thus effectively defining a point to
//         be 1.333 pixels. Additionally, the TrueType font data provides
//         an explicit scale factor to scale a given font's glyphs to points,
//         but the author has observed that this scale factor is often wrong
//         for non-commercial fonts, thus making fonts scaled in points
//         according to the TrueType spec incoherently sized in practice.
//
// DETAILED USAGE:
//
//  Scale:
//    Select how high you want the font to be, in points or pixels.
//    Call ScaleForPixelHeight or ScaleForMappingEmToPixels to compute
//    a scale factor SF that will be used by all other functions.
//
//  Baseline:
//    You need to select a y-coordinate that is the baseline of where
//    your text will appear. Call GetFontBoundingBox to get the baseline-relative
//    bounding box for all characters. SF*-y0 will be the distance in pixels
//    that the worst-case character could extend above the baseline, so if
//    you want the top edge of characters to appear at the top of the
//    screen where y=0, then you would set the baseline to SF*-y0.
//
//  Current point:
//    Set the current point where the first character will appear. The
//    first character could extend left of the current point; this is font
//    dependent. You can either choose a current point that is the leftmost
//    point and hope, or add some padding, or check the bounding box or
//    left-side-bearing of the first character to be displayed and set
//    the current point based on that.
//
//  Displaying a character:
//    Compute the bounding box of the character. It will contain signed values
//    relative to <current_point, baseline>. I.e. if it returns x0,y0,x1,y1,
//    then the character should be displayed in the rectangle from
//    <current_point+SF*x0, baseline+SF*y0> to <current_point+SF*x1,baseline+SF*y1).
//
//  Advancing for the next character:
//    Call GlyphHMetrics, and compute 'current_point += SF * advance'.
//
//
// ADVANCED USAGE
//
//   Quality:
//
//    - Use the functions with Subpixel at the end to allow your characters
//      to have subpixel positioning. Since the font is anti-aliased, not
//      hinted, this is very import for quality. (This is not possible with
//      baked fonts.)
//
//    - Kerning is now supported, and if you're supporting subpixel rendering
//      then kerning is worth using to give your text a polished look.
//
//   Performance:
//
//    - Convert Unicode codepoints to glyph indexes and operate on the glyphs;
//      if you don't do this, stb_truetype is forced to do the conversion on
//      every call.
//
//    - There are a lot of memory allocations. We should modify it to take
//      a temp buffer and allocate from the temp buffer (without freeing),
//      should help performance a lot.
//
// NOTES
//
//   The system uses the raw data found in the .ttf file without changing it
//   and without building auxiliary data structures. This is a bit inefficient
//   on little-endian systems (the data is big-endian), but assuming you're
//   caching the bitmaps or glyph shapes this shouldn't be a big deal.
//
//   It appears to be very hard to programmatically determine what font a
//   given file is in a general way. I provide an API for this, but I don't
//   recommend it.
//
//
// PERFORMANCE MEASUREMENTS FOR 1.06:
//
//                      32-bit     64-bit
//   Previous release:  8.83 s     7.68 s
//   Pool allocations:  7.72 s     6.34 s
//   Inline sort     :  6.54 s     5.65 s
//   New rasterizer  :  5.63 s     5.00 s

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   INTERFACE
////
////

#ifdef STBTT_STATIC
#define STBTT_DEF static
#else
#define STBTT_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

// private structure
typedef struct
{
   unsigned char *data;
   int cursor;
   int size;
} stbtt__buf;

//////////////////////////////////////////////////////////////////////////////
//
// TEXTURE BAKING API
//
// If you use this API, you only have to call two functions ever.
//

typedef struct
{
   unsigned short x0,y0,x1,y1; // coordinates of bbox in bitmap
   float xoff,yoff,xadvance;
} stbtt_bakedchar;

STBTT_DEF int stbtt_BakeFontBitmap(const unsigned char *data, int offset,  // font location (use offset=0 for plain .ttf)
                                float pixel_height,                     // height of font in pixels
                                unsigned char *pixels, int pw, int ph,  // bitmap to be filled in
                                int first_char, int num_chars,          // characters to bake
                                stbtt_bakedchar *chardata);             // you allocate this, it's num_chars long
// if return is positive, the first unused row of the bitmap
// if return is negative, returns the negative of the number of characters that fit
// if return is 0, no characters fit and no rows were used
// This uses a very crappy packing.

typedef struct
{
   float x0,y0,s0,t0; // top-left
   float x1,y1,s1,t1; // bottom-right
} stbtt_aligned_quad;

STBTT_DEF void stbtt_GetBakedQuad(const stbtt_bakedchar *chardata, int pw, int ph,  // same data as above
                               int char_index,             // character to display
                               float *xpos, float *ypos,   // pointers to current position in screen pixel space
                               stbtt_aligned_quad *q,      // output: quad to draw
                               int opengl_fillrule);       // true if opengl fill rule; false if DX9 or earlier
// Call GetBakedQuad with char_index = 'character - first_char', and it
// creates the quad you need to draw and advances the current position.
//
// The coordinate system used assumes y increases downwards.
//
// Characters will extend both above and below the current position;
// see discussion of "BASELINE" above.
//
// It's inefficient; you might want to c&p it and optimize it.

STBTT_DEF void stbtt_GetScaledFontVMetrics(const unsigned char *fontdata, int index, float size, float *ascent, float *descent, float *lineGap);
// Query the font vertical metrics without having to create a font first.


//////////////////////////////////////////////////////////////////////////////
//
// NEW TEXTURE BAKING API
//
// This provides options for packing multiple fonts into one atlas, not
// perfectly but better than nothing.

typedef struct
{
   unsigned short x0,y0,x1,y1; // coordinates of bbox in bitmap
   float xoff,yoff,xadvance;
   float xoff2,yoff2;
} stbtt_packedchar;

typedef struct stbtt_pack_context stbtt_pack_context;
typedef struct stbtt_fontinfo stbtt_fontinfo;
#ifndef STB_RECT_PACK_VERSION
typedef struct stbrp_rect stbrp_rect;
#endif

STBTT_DEF int  stbtt_PackBegin(stbtt_pack_context *spc, unsigned char *pixels, int width, int height, int stride_in_bytes, int padding, void *alloc_context);
// Initializes a packing context stored in the passed-in stbtt_pack_context.
// Future calls using this context will pack characters into the bitmap passed
// in here: a 1-channel bitmap that is width * height. stride_in_bytes is
// the distance from one row to the next (or 0 to mean they are packed tightly
// together). "padding" is the amount of padding to leave between each
// character (normally you want '1' for bitmaps you'll use as textures with
// bilinear filtering).
//
// Returns 0 on failure, 1 on success.

STBTT_DEF void stbtt_PackEnd  (stbtt_pack_context *spc);
// Cleans up the packing context and frees all memory.

#define STBTT_POINT_SIZE(x)   (-(x))

STBTT_DEF int  stbtt_PackFontRange(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, float font_size,
                                int first_unicode_char_in_range, int num_chars_in_range, stbtt_packedchar *chardata_for_range);
// Creates character bitmaps from the font_index'th font found in fontdata (use
// font_index=0 if you don't know what that is). It creates num_chars_in_range
// bitmaps for characters with unicode values starting at first_unicode_char_in_range
// and increasing. Data for how to render them is stored in chardata_for_range;
// pass these to stbtt_GetPackedQuad to get back renderable quads.
//
// font_size is the full height of the character from ascender to descender,
// as computed by stbtt_ScaleForPixelHeight. To use a point size as computed
// by stbtt_ScaleForMappingEmToPixels, wrap the point size in STBTT_POINT_SIZE()
// and pass that result as 'font_size':
//       ...,                  20 , ... // font max minus min y is 20 pixels tall
//       ..., STBTT_POINT_SIZE(20), ... // 'M' is 20 pixels tall

typedef struct
{
   float font_size;
   int first_unicode_codepoint_in_range;  // if non-zero, then the chars are continuous, and this is the first codepoint
   int *array_of_unicode_codepoints;       // if non-zero, then this is an array of unicode codepoints
   int num_chars;
   stbtt_packedchar *chardata_for_range; // output
   unsigned char h_oversample, v_oversample; // don't set these, they're used internally
} stbtt_pack_range;

STBTT_DEF int  stbtt_PackFontRanges(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, stbtt_pack_range *ranges, int num_ranges);
// Creates character bitmaps from multiple ranges of characters stored in
// ranges. This will usually create a better-packed bitmap than multiple
// calls to stbtt_PackFontRange. Note that you can call this multiple
// times within a single PackBegin/PackEnd.

STBTT_DEF void stbtt_PackSetOversampling(stbtt_pack_context *spc, unsigned int h_oversample, unsigned int v_oversample);
// Oversampling a font increases the quality by allowing higher-quality subpixel
// positioning, and is especially valuable at smaller text sizes.
//
// This function sets the amount of oversampling for all following calls to
// stbtt_PackFontRange(s) or stbtt_PackFontRangesGatherRects for a given
// pack context. The default (no oversampling) is achieved by h_oversample=1
// and v_oversample=1. The total number of pixels required is
// h_oversample*v_oversample larger than the default; for example, 2x2
// oversampling requires 4x the storage of 1x1. For best results, render
// oversampled textures with bilinear filtering. Look at the readme in
// stb/tests/oversample for information about oversampled fonts
//
// To use with PackFontRangesGather etc., you must set it before calls
// call to PackFontRangesGatherRects.

STBTT_DEF void stbtt_PackSetSkipMissingCodepoints(stbtt_pack_context *spc, int skip);
// If skip != 0, this tells stb_truetype to skip any codepoints for which
// there is no corresponding glyph. If skip=0, which is the default, then
// codepoints without a glyph recived the font's "missing character" glyph,
// typically an empty box by convention.

STBTT_DEF void stbtt_GetPackedQuad(const stbtt_packedchar *chardata, int pw, int ph,  // same data as above
                               int char_index,             // character to display
                               float *xpos, float *ypos,   // pointers to current position in screen pixel space
                               stbtt_aligned_quad *q,      // output: quad to draw
                               int align_to_integer);

STBTT_DEF int  stbtt_PackFontRangesGatherRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects);
STBTT_DEF void stbtt_PackFontRangesPackRects(stbtt_pack_context *spc, stbrp_rect *rects, int num_rects);
STBTT_DEF int  stbtt_PackFontRangesRenderIntoRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects);
// Calling these functions in sequence is roughly equivalent to calling
// stbtt_PackFontRanges(). If you more control over the packing of multiple
// fonts, or if you want to pack custom data into a font texture, take a look
// at the source to of stbtt_PackFontRanges() and create a custom version
// using these functions, e.g. call GatherRects multiple times,
// building up a single array of rects, then call PackRects once,
// then call RenderIntoRects repeatedly. This may result in a
// better packing than calling PackFontRanges multiple times
// (or it may not).

// this is an opaque structure that you shouldn't mess with which holds
// all the context needed from PackBegin to PackEnd.
struct stbtt_pack_context {
   void *user_allocator_context;
   void *pack_info;
   int   width;
   int   height;
   int   stride_in_bytes;
   int   padding;
   int   skip_missing;
   unsigned int   h_oversample, v_oversample;
   unsigned char *pixels;
   void  *nodes;
};

//////////////////////////////////////////////////////////////////////////////
//
// FONT LOADING
//
//

STBTT_DEF int stbtt_GetNumberOfFonts(const unsigned char *data);
// This function will determine the number of fonts in a font file.  TrueType
// collection (.ttc) files may contain multiple fonts, while TrueType font
// (.ttf) files only contain one font. The number of fonts can be used for
// indexing with the previous function where the index is between zero and one
// less than the total fonts. If an error occurs, -1 is returned.

STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *data, int index);
// Each .ttf/.ttc file may have more than one font. Each font has a sequential
// index number starting from 0. Call this function to get the font offset for
// a given index; it returns -1 if the index is out of range. A regular .ttf
// file will only define one font and it always be at offset 0, so it will
// return '0' for index 0, and -1 for all other indices.

// The following structure is defined publicly so you can declare one on
// the stack or as a global or etc, but you should treat it as opaque.
struct stbtt_fontinfo
{
   void           * userdata;
   unsigned char  * data;              // pointer to .ttf file
   int              fontstart;         // offset of start of font

   int numGlyphs;                     // number of glyphs, needed for range checking

   int loca,head,glyf,hhea,hmtx,kern,gpos,svg; // table locations as offset from start of .ttf
   int index_map;                     // a cmap mapping for our chosen character encoding
   int indexToLocFormat;              // format needed to map from glyph index to glyph

   stbtt__buf cff;                    // cff font data
   stbtt__buf charstrings;            // the charstring index
   stbtt__buf gsubrs;                 // global charstring subroutines index
   stbtt__buf subrs;                  // private charstring subroutines index
   stbtt__buf fontdicts;              // array of font dicts
   stbtt__buf fdselect;               // map from glyph to fontdict
};

STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset);
// Given an offset into the file that defines a font, this function builds
// the necessary cached info for the rest of the system. You must allocate
// the stbtt_fontinfo yourself, and stbtt_InitFont will fill it out. You don't
// need to do anything special to free it, because the contents are pure
// value data with no additional data structures. Returns 0 on failure.


//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER TO GLYPH-INDEX CONVERSIOn

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint);
// If you're going to perform multiple operations on the same character
// and you want a speed-up, call this function with the character you're
// going to process, then use glyph-based functions instead of the
// codepoint-based functions.
// Returns 0 if the character codepoint is not defined in the font.


//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER PROPERTIES
//

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);
// computes a scale factor to produce a font whose "height" is 'pixels' tall.
// Height is measured as the distance from the highest ascender to the lowest
// descender; in other words, it's equivalent to calling stbtt_GetFontVMetrics
// and computing:
//       scale = pixels / (ascent - descent)
// so if you prefer to measure height by the ascent only, use a similar calculation.

STBTT_DEF float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels);
// computes a scale factor to produce a font whose EM size is mapped to
// 'pixels' tall. This is probably what traditional APIs compute, but
// I'm not positive.

STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap);
// ascent is the coordinate above the baseline the font extends; descent
// is the coordinate below the baseline the font extends (i.e. it is typically negative)
// lineGap is the spacing between one row's descent and the next row's ascent...
// so you should advance the vertical position by "*ascent - *descent + *lineGap"
//   these are expressed in unscaled coordinates, so you must multiply by
//   the scale factor for a given size

STBTT_DEF int  stbtt_GetFontVMetricsOS2(const stbtt_fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap);
// analogous to GetFontVMetrics, but returns the "typographic" values from the OS/2
// table (specific to MS/Windows TTF files).
//
// Returns 1 on success (table present), 0 on failure.

STBTT_DEF void stbtt_GetFontBoundingBox(const stbtt_fontinfo *info, int *x0, int *y0, int *x1, int *y1);
// the bounding box around all possible characters

STBTT_DEF void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing);
// leftSideBearing is the offset from the current horizontal position to the left edge of the character
// advanceWidth is the offset from the current horizontal position to the next horizontal position
//   these are expressed in unscaled coordinates

STBTT_DEF int  stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info, int ch1, int ch2);
// an additional amount to add to the 'advance' value between ch1 and ch2

STBTT_DEF int stbtt_GetCodepointBox(const stbtt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1);
// Gets the bounding box of the visible part of the glyph, in unscaled coordinates

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing);
STBTT_DEF int  stbtt_GetGlyphKernAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2);
STBTT_DEF int  stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);
// as above, but takes one or more glyph indices for greater efficiency

typedef struct stbtt_kerningentry
{
   int glyph1; // use stbtt_FindGlyphIndex
   int glyph2;
   int advance;
} stbtt_kerningentry;

STBTT_DEF int  stbtt_GetKerningTableLength(const stbtt_fontinfo *info);
STBTT_DEF int  stbtt_GetKerningTable(const stbtt_fontinfo *info, stbtt_kerningentry* table, int table_length);
// Retrieves a complete list of all of the kerning pairs provided by the font
// stbtt_GetKerningTable never writes more than table_length entries and returns how many entries it did write.
// The table will be sorted by (a.glyph1 == b.glyph1)?(a.glyph2 < b.glyph2):(a.glyph1 < b.glyph1)

//////////////////////////////////////////////////////////////////////////////
//
// GLYPH SHAPES (you probably don't need these, but they have to go before
// the bitmaps for C declaration-order reasons)
//

#ifndef STBTT_vmove // you can predefine these to use different values (but why?)
   enum {
      STBTT_vmove=1,
      STBTT_vline,
      STBTT_vcurve,
      STBTT_vcubic
   };
#endif

#ifndef stbtt_vertex // you can predefine this to use different values
                   // (we share this with other code at RAD)
   #define stbtt_vertex_type short // can't use stbtt_int16 because that's not visible in the header file
   typedef struct
   {
      stbtt_vertex_type x,y,cx,cy,cx1,cy1;
      unsigned char type,padding;
   } stbtt_vertex;
#endif

STBTT_DEF int stbtt_IsGlyphEmpty(const stbtt_fontinfo *info, int glyph_index);
// returns non-zero if nothing is drawn for this glyph

STBTT_DEF int stbtt_GetCodepointShape(const stbtt_fontinfo *info, int unicode_codepoint, stbtt_vertex **vertices);
STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **vertices);
// returns # of vertices and fills *vertices with the pointer to them
//   these are expressed in "unscaled" coordinates
//
// The shape is a series of contours. Each one starts with
// a STBTT_moveto, then consists of a series of mixed
// STBTT_lineto and STBTT_curveto segments. A lineto
// draws a line from previous endpoint to its x,y; a curveto
// draws a quadratic bezier from previous endpoint to
// its x,y, using cx,cy as the bezier control point.

STBTT_DEF void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *vertices);
// frees the data allocated above

STBTT_DEF unsigned char *stbtt_FindSVGDoc(const stbtt_fontinfo *info, int gl);
STBTT_DEF int stbtt_GetCodepointSVG(const stbtt_fontinfo *info, int unicode_codepoint, const char **svg);
STBTT_DEF int stbtt_GetGlyphSVG(const stbtt_fontinfo *info, int gl, const char **svg);
// fills svg with the character's SVG data.
// returns data size or 0 if SVG not found.

//////////////////////////////////////////////////////////////////////////////
//
// BITMAP RENDERING
//

STBTT_DEF void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata);
// frees the bitmap allocated below

STBTT_DEF unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff);
// allocates a large-enough single-channel 8bpp bitmap and renders the
// specified character/glyph at the specified scale into it, with
// antialiasing. 0 is no coverage (transparent), 255 is fully covered (opaque).
// *width & *height are filled out with the width & height of the bitmap,
// which is stored left-to-right, top-to-bottom.
//
// xoff/yoff are the offset it pixel space from the glyph origin to the top-left of the bitmap

STBTT_DEF unsigned char *stbtt_GetCodepointBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint, int *width, int *height, int *xoff, int *yoff);
// the same as stbtt_GetCodepoitnBitmap, but you can specify a subpixel
// shift for the character

STBTT_DEF void stbtt_MakeCodepointBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int codepoint);
// the same as stbtt_GetCodepointBitmap, but you pass in storage for the bitmap
// in the form of 'output', with row spacing of 'out_stride' bytes. the bitmap
// is clipped to out_w/out_h bytes. Call stbtt_GetCodepointBitmapBox to get the
// width and height and positioning info for it first.

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint);
// same as stbtt_MakeCodepointBitmap, but you can specify a subpixel
// shift for the character

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int codepoint);
// same as stbtt_MakeCodepointBitmapSubpixel, but prefiltering
// is performed (see stbtt_PackSetOversampling)

STBTT_DEF void stbtt_GetCodepointBitmapBox(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
// get the bbox of the bitmap centered around the glyph origin; so the
// bitmap width is ix1-ix0, height is iy1-iy0, and location to place
// the bitmap top left is (leftSideBearing*scale,iy0).
// (Note that the bitmap uses y-increases-down, but the shape uses
// y-increases-up, so CodepointBitmapBox and CodepointBox are inverted.)

STBTT_DEF void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);
// same as stbtt_GetCodepointBitmapBox, but you can specify a subpixel
// shift for the character

// the following functions are equivalent to the above functions, but operate
// on glyph indices instead of Unicode codepoints (for efficiency)
STBTT_DEF unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff);
STBTT_DEF unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int *width, int *height, int *xoff, int *yoff);
STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph);
STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph);
STBTT_DEF void stbtt_MakeGlyphBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int glyph);
STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y,float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);


// @TODO: don't expose this structure
typedef struct
{
   int w,h,stride;
   unsigned char *pixels;
} stbtt__bitmap;

// rasterize a shape with quadratic beziers into a bitmap
STBTT_DEF void stbtt_Rasterize(stbtt__bitmap *result,        // 1-channel bitmap to draw into
                               float flatness_in_pixels,     // allowable error of curve in pixels
                               stbtt_vertex *vertices,       // array of vertices defining shape
                               int num_verts,                // number of vertices in above array
                               float scale_x, float scale_y, // scale applied to input vertices
                               float shift_x, float shift_y, // translation applied to input vertices
                               int x_off, int y_off,         // another translation applied to input
                               int invert,                   // if non-zero, vertically flip shape
                               void *userdata);              // context for to STBTT_MALLOC

//////////////////////////////////////////////////////////////////////////////
//
// Signed Distance Function (or Field) rendering

STBTT_DEF void stbtt_FreeSDF(unsigned char *bitmap, void *userdata);
// frees the SDF bitmap allocated below

STBTT_DEF unsigned char * stbtt_GetGlyphSDF(const stbtt_fontinfo *info, float scale, int glyph, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff);
STBTT_DEF unsigned char * stbtt_GetCodepointSDF(const stbtt_fontinfo *info, float scale, int codepoint, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff);
// These functions compute a discretized SDF field for a single character, suitable for storing
// in a single-channel texture, sampling with bilinear filtering, and testing against
// larger than some threshold to produce scalable fonts.
//        info              --  the font
//        scale             --  controls the size of the resulting SDF bitmap, same as it would be creating a regular bitmap
//        glyph/codepoint   --  the character to generate the SDF for
//        padding           --  extra "pixels" around the character which are filled with the distance to the character (not 0),
//                                 which allows effects like bit outlines
//        onedge_value      --  value 0-255 to test the SDF against to reconstruct the character (i.e. the isocontour of the character)
//        pixel_dist_scale  --  what value the SDF should increase by when moving one SDF "pixel" away from the edge (on the 0..255 scale)
//                                 if positive, > onedge_value is inside; if negative, < onedge_value is inside
//        width,height      --  output height & width of the SDF bitmap (including padding)
//        xoff,yoff         --  output origin of the character
//        return value      --  a 2D array of bytes 0..255, width*height in size
//
// pixel_dist_scale & onedge_value are a scale & bias that allows you to make
// optimal use of the limited 0..255 for your application, trading off precision
// and special effects. SDF values outside the range 0..255 are clamped to 0..255.
//
// Example:
//      scale = stbtt_ScaleForPixelHeight(22)
//      padding = 5
//      onedge_value = 180
//      pixel_dist_scale = 180/5.0 = 36.0
//
//      This will create an SDF bitmap in which the character is about 22 pixels
//      high but the whole bitmap is about 22+5+5=32 pixels high. To produce a filled
//      shape, sample the SDF at each pixel and fill the pixel if the SDF value
//      is greater than or equal to 180/255. (You'll actually want to antialias,
//      which is beyond the scope of this example.) Additionally, you can compute
//      offset outlines (e.g. to stroke the character border inside & outside,
//      or only outside). For example, to fill outside the character up to 3 SDF
//      pixels, you would compare against (180-36.0*3)/255 = 72/255. The above
//      choice of variables maps a range from 5 pixels outside the shape to
//      2 pixels inside the shape to 0..255; this is intended primarily for apply
//      outside effects only (the interior range is needed to allow proper
//      antialiasing of the font at *smaller* sizes)
//
// The function computes the SDF analytically at each SDF pixel, not by e.g.
// building a higher-res bitmap and approximating it. In theory the quality
// should be as high as possible for an SDF of this size & representation, but
// unclear if this is true in practice (perhaps building a higher-res bitmap
// and computing from that can allow drop-out prevention).
//
// The algorithm has not been optimized at all, so expect it to be slow
// if computing lots of characters or very large sizes.



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
//     stbtt_FindMatchingFont() will use *case-sensitive* comparisons on
//             unicode-encoded names to try to find the font you want;
//             you can run this before calling stbtt_InitFont()
//
//     stbtt_GetFontNameString() lets you get any of the various strings
//             from the file yourself and do your own comparisons on them.
//             You have to have called stbtt_InitFont() first.


STBTT_DEF int stbtt_FindMatchingFont(const unsigned char *fontdata, const char *name, int flags);
// returns the offset (not index) of the font that matches, or -1 if none
//   if you use STBTT_MACSTYLE_DONTCARE, use a font name like "Arial Bold".
//   if you use any other flag, use a font name like "Arial"; this checks
//     the 'macStyle' header field; i don't know if fonts set this consistently
#define STBTT_MACSTYLE_DONTCARE     0
#define STBTT_MACSTYLE_BOLD         1
#define STBTT_MACSTYLE_ITALIC       2
#define STBTT_MACSTYLE_UNDERSCORE   4
#define STBTT_MACSTYLE_NONE         8   // <= not same as 0, this makes us check the bitfield is 0

STBTT_DEF int stbtt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2);
// returns 1/0 whether the first string interpreted as utf8 is identical to
// the second string interpreted as big-endian utf16... useful for strings from next func

STBTT_DEF const char *stbtt_GetFontNameString(const stbtt_fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID);
// returns the string (which may be big-endian double byte, e.g. for unicode)
// and puts the length in bytes in *length.
//
// some of the values for the IDs are below; for more see the truetype spec:
//     http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6name.html
//     http://www.microsoft.com/typography/otspec/name.htm

enum { // platformID
   STBTT_PLATFORM_ID_UNICODE   =0,
   STBTT_PLATFORM_ID_MAC       =1,
   STBTT_PLATFORM_ID_ISO       =2,
   STBTT_PLATFORM_ID_MICROSOFT =3
};

enum { // encodingID for STBTT_PLATFORM_ID_UNICODE
   STBTT_UNICODE_EID_UNICODE_1_0    =0,
   STBTT_UNICODE_EID_UNICODE_1_1    =1,
   STBTT_UNICODE_EID_ISO_10646      =2,
   STBTT_UNICODE_EID_UNICODE_2_0_BMP=3,
   STBTT_UNICODE_EID_UNICODE_2_0_FULL=4
};

enum { // encodingID for STBTT_PLATFORM_ID_MICROSOFT
   STBTT_MS_EID_SYMBOL        =0,
   STBTT_MS_EID_UNICODE_BMP   =1,
   STBTT_MS_EID_SHIFTJIS      =2,
   STBTT_MS_EID_UNICODE_FULL  =10
};

enum { // encodingID for STBTT_PLATFORM_ID_MAC; same as Script Manager codes
   STBTT_MAC_EID_ROMAN        =0,   STBTT_MAC_EID_ARABIC       =4,
   STBTT_MAC_EID_JAPANESE     =1,   STBTT_MAC_EID_HEBREW       =5,
   STBTT_MAC_EID_CHINESE_TRAD =2,   STBTT_MAC_EID_GREEK        =6,
   STBTT_MAC_EID_KOREAN       =3,   STBTT_MAC_EID_RUSSIAN      =7
};

enum { // languageID for STBTT_PLATFORM_ID_MICROSOFT; same as LCID...
       // problematic because there are e.g. 16 english LCIDs and 16 arabic LCIDs
   STBTT_MS_LANG_ENGLISH     =0x0409,   STBTT_MS_LANG_ITALIAN     =0x0410,
   STBTT_MS_LANG_CHINESE     =0x0804,   STBTT_MS_LANG_JAPANESE    =0x0411,
   STBTT_MS_LANG_DUTCH       =0x0413,   STBTT_MS_LANG_KOREAN      =0x0412,
   STBTT_MS_LANG_FRENCH      =0x040c,   STBTT_MS_LANG_RUSSIAN     =0x0419,
   STBTT_MS_LANG_GERMAN      =0x0407,   STBTT_MS_LANG_SPANISH     =0x0409,
   STBTT_MS_LANG_HEBREW      =0x040d,   STBTT_MS_LANG_SWEDISH     =0x041D
};

enum { // languageID for STBTT_PLATFORM_ID_MAC
   STBTT_MAC_LANG_ENGLISH      =0 ,   STBTT_MAC_LANG_JAPANESE     =11,
   STBTT_MAC_LANG_ARABIC       =12,   STBTT_MAC_LANG_KOREAN       =23,
   STBTT_MAC_LANG_DUTCH        =4 ,   STBTT_MAC_LANG_RUSSIAN      =32,
   STBTT_MAC_LANG_FRENCH       =1 ,   STBTT_MAC_LANG_SPANISH      =6 ,
   STBTT_MAC_LANG_GERMAN       =2 ,   STBTT_MAC_LANG_SWEDISH      =5 ,
   STBTT_MAC_LANG_HEBREW       =10,   STBTT_MAC_LANG_CHINESE_SIMPLIFIED =33,
   STBTT_MAC_LANG_ITALIAN      =3 ,   STBTT_MAC_LANG_CHINESE_TRAD =19
};

#ifdef __cplusplus
}
#endif

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
} Style;

typedef struct FrameBuffer {
    uint8_t *data; // may be larger than screen, to avoid frequent reallocations
    // width, height tracked instead of flat size because it scales along with sdl texture, which can't be reshaped
    size_t max_width, max_height;
    Image image; // target to draw to framebuffer. links to FrameBuffer.data
    SDL_Texture *texture;
} FrameBuffer;

typedef struct RenderContext {
    stbtt_fontinfo font;
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
