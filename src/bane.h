#ifndef BANE_H
#define BANE_H

#include <stdint.h>
#include "raylib.h"

#define STATUS_LENGTH 512

typedef struct {
	int code;
	char message[STATUS_LENGTH];
} SS_Status;

typedef struct {
	uint32_t key;
	int x, y, w, h, origin_x, origin_y; // origin not used internally. Can be used to position texture on rendering.
} TextureRect;

typedef struct {
    TextureRect *items;
    int count, cap;
} TextureRectArray;

typedef struct {
    int x;
    int y;
} IntVec2;

typedef struct {
    IntVec2 *items;
    int count, cap;
} IntVec2Array;

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

typedef enum { TA_OK = 0, TA_MAX_SIZE_EXCEEDED = 1, TA_RECT_NOT_FOUND = 2 } TA_Status;

// square packing area starts out small, then expands until it reaches a side length of max_size
TextureAtlas* texture_atlas_create(int max_size); 
void texture_atlas_destroy(TextureAtlas **texture_atlas);

// Add a new rect to your texture atlas or returns existing rect if key exists already.
TA_Status texture_atlas_add_get_rect(TextureRect *return_rect, TextureAtlas *texture_atlas, uint32_t key, Image texture, int origin_x, int origin_y);
// TA_Status texture_atlas_get_rect(TextureRect *return_rect, const TextureAtlas *texture_atlas, uint32_t key);

// Return atlas texture on GPU. Updates it before if necessary
Texture2D texture_atlas_get_texture(TextureAtlas *texture_atlas);
TA_Status texture_atlas_draw(TextureAtlas *texture_atlas, uint32_t key, int x, int y, Color tint);

#endif
