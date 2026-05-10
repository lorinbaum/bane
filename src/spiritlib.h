#ifndef SPIRITLIB_H
#define SPIRITLIB_H

#include <stdint.h>
#include "raylib.h"

#define STATUS_LENGTH 256

typedef struct {
	int code;
	char message[STATUS_LENGTH];
} SS_Status;

typedef struct TextureAtlas TextureAtlas;
typedef enum { TA_OK = 0, TA_MAX_SIZE_EXCEEDED = 1, TA_RECT_NOT_FOUND = 2 } TA_Status;


typedef struct {
	uint32_t key;
	int x, y, w, h, origin_x, origin_y; // origin not used internally. Can be used to position texture on rendering.
} TextureRect;

// square packing area starts out small, then expands until it reaches a side length of max_size
TextureAtlas* texture_atlas_create(int max_size); 
void texture_atlas_destroy(TextureAtlas **texture_atlas);

// Add a new rect to your texture atlas or returns existing rect if key exists already.
TA_Status texture_atlas_add_get_rect(TextureRect *return_rect, TextureAtlas *texture_atlas, uint32_t key, uint8_t *texture, int w, int h, int origin_x, int origin_y);
TA_Status texture_atlas_get_rect(TextureRect *return_rect, const TextureAtlas *texture_atlas, uint32_t key);

// Return atlas texture on GPU. Updates it before if necessary
Texture2D texture_atlas_get_texture(TextureAtlas *texture_atlas);
TA_Status texture_atlas_draw(TextureAtlas *texture_atlas, uint32_t key, int x, int y, Color tint);

SS_Status texture_atlas_write_bmp(const TextureAtlas *texture_atlas, const char* path);

// IMAGE

typedef enum { BMP_OK = 0, BMP_FOPEN_ERROR = 1, BMP_FWRITE_ERROR = 2, BMP_ARGUMENT_ERROR = 4, BMP_SIZE_ERROR = 5 } bmp_status;

SS_Status bmp_write(const char *path, const uint8_t *data, uint32_t w, uint32_t h, uint32_t c);

#endif
