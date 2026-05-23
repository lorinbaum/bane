#ifndef BANE_H
#define BANE_H

#include <stdlib.h>
#include <stdint.h>
#include "raylib.h"
#include <assert.h>

typedef struct {
	uint32_t key; // 32 instead of 16 for easiser key generation: character keycode can be used in full
	uint16_t x, y, w, h, origin_x, origin_y; // origin not used internally. Can be used to position texture on rendering.
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
        unsigned int count, cap; \
    } STRUCT##Array; \
    STRUCT##Array create_##STRUCT##Array(uint cap); \
    void destroy_##STRUCT##Array(STRUCT##Array *a); \
    void expand_##STRUCT##Array(STRUCT##Array *a); \
    void append_##STRUCT(STRUCT##Array *a, TYPEOF(*a->items) item); \
    void delete_##STRUCT(STRUCT##Array *a, unsigned int index); \
    void insert_##STRUCT(STRUCT##Array *a, unsigned int index, TYPEOF(*a->items) item);

#define ARRAY_DEFINE(STRUCT) \
    STRUCT##Array create_##STRUCT##Array(uint cap) { \
        STRUCT##Array ret = (STRUCT##Array) {malloc(cap * sizeof(STRUCT)), 0, cap}; \
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
            a->cap = a->cap > 0 ? a->cap * 2 : 16; \
            a->items = realloc(a->items, sizeof(a->items[0]) * a->cap); \
            ensure(a->items != NULL); \
        }\
    } \
    void append_##STRUCT(STRUCT##Array *a, TYPEOF(*a->items) item) { \
        expand_##STRUCT##Array(a); \
        a->items[a->count] = item; \
        a->count++; \
    } \
    void delete_##STRUCT(STRUCT##Array *a, unsigned int index) { \
        assert(index < a->count); \
        for (unsigned int i = index; i + 1 < a->count; i++) { a->items[i] = a->items[i+1]; } \
        a->count--; \
    } \
    void insert_##STRUCT(STRUCT##Array *a, unsigned int index, TYPEOF(*a->items) item) { \
        assert(index <= a->count); \
        expand_##STRUCT##Array(a); \
        for (unsigned i = a->count; i > index; i--) { a->items[i] = a->items[i-1]; } \
        a->items[index] = item; \
        a->count++; \
    }

static inline int imin(int a, int b) { return a > b ? b : a; }
static inline int imax(int a, int b) { return a > b ? a : b; }

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

#endif
