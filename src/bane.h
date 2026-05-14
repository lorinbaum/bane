#ifndef BANE_H
#define BANE_H

#include <stdint.h>
#include "raylib.h"
#include <assert.h>

// requires GCC or CLANG because __typeof__ not in std=c17
#define TYPEOF __typeof__

#define ARRAY_AUTO_EXPAND(STRUCT) \
    typedef struct {\
        STRUCT *items; \
        unsigned int count, cap; \
    } STRUCT##Array; \
    static inline void auto_expand_##STRUCT##Array(STRUCT##Array *a) { \
        if (a->count >= a->cap) { \
            a->cap = a->cap > 0 ? a->cap * 2 : 16; \
            a->items = realloc(a->items, sizeof(a->items[0]) * a->cap); \
            ensure(a->items != NULL); \
        }\
    }

#define ARRAY_APPEND(STRUCT) \
    static inline void append_##STRUCT(STRUCT *a, TYPEOF(*a->items) item) { \
        auto_expand_##STRUCT(a); \
        a->items[a->count] = item; \
        a->count++; \
    }
    
#define ARRAY_DELETE(STRUCT) \
    static inline void delete_##STRUCT(STRUCT *a, unsigned int index) { \
        assert(index < a->count); \
        for (unsigned int i = index; i + 1 < a->count; i++) { a->items[i] = a->items[i+1]; } \
        a->count--; \
    }

#define ARRAY_INSERT(STRUCT) \
    static inline void insert_##STRUCT(STRUCT *a, unsigned int index, TYPEOF(*a->items) item) { \
        assert(index <= a->count); \
        auto_expand_##STRUCT(a); \
        for (unsigned i = a->count; i > index; i--) { a->items[i] = a->items[i-1]; } \
        a->items[index] = item; \
        a->count++; \
    }

static inline int imin(int a, int b) { return a > b ? b : a; }
static inline int imax(int a, int b) { return a > b ? a : b; }

void ensure_fail(const char *file, int line, const char *func, const char *expr);
#define ensure(expr) ((expr) ? (void) (0) : ensure_fail(__FILE__, __LINE__, __func__, #expr))

typedef struct {
	uint32_t key;
	int x, y, w, h, origin_x, origin_y; // origin not used internally. Can be used to position texture on rendering.
} TextureRect;

ARRAY_AUTO_EXPAND(TextureRect)
ARRAY_APPEND(TextureRectArray)

typedef struct {
    int x;
    int y;
} IntVec2;

ARRAY_AUTO_EXPAND(IntVec2)
ARRAY_APPEND(IntVec2Array)
ARRAY_DELETE(IntVec2Array)
ARRAY_INSERT(IntVec2Array)

#define DEFAULT_TEXTURE_ATLAS_SIZE 64
#define DEFAULT_SKYLINE_ANCHOR_CAP 32
#define DEFAULT_TEXTURE_RECTS_CAP 64

typedef struct {
    int channels;
    int size; // texture area is square of side length "size".
    int max_size; // hardware constrained.
    IntVec2Array skyline_anchors;
    TextureRectArray rects;
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
// TAStatus texture_atlas_get_rect(TextureRect *return_rect, const TextureAtlas *texture_atlas, uint32_t key);

void texture_atlas_update_texture(TextureAtlas *texture_atlas);
TAStatus texture_atlas_draw(TextureAtlas *texture_atlas, uint32_t key, int x, int y, Color tint);

#endif
