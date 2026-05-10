#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "bane.h"
#include "helpers.h"

#define DEFAULT_TEXTURE_ATLAS_SIZE 64
#define DEFAULT_SKYLINE_ANCHOR_CAP 32
#define DEFAULT_TEXTURE_RECTS_CAP 64

typedef struct { int x, y, index; } Anchor;

ARRAY_AUTO_EXPAND(IntVec2Array);
ARRAY_APPEND(IntVec2Array);
ARRAY_DELETE(IntVec2Array);
ARRAY_INSERT(IntVec2Array)

ARRAY_AUTO_EXPAND(TextureRectArray);
ARRAY_APPEND(TextureRectArray);

TextureAtlas *texture_atlas_create(int max_size) {
    assert(max_size > 0);
    TextureAtlas *texture_atlas = calloc(1, sizeof(TextureAtlas));
    ensure(texture_atlas != NULL);
    texture_atlas->channels = 1; // start of future expansion to 3 channels support. currently only 1 supported.
    texture_atlas->size = imin(DEFAULT_TEXTURE_ATLAS_SIZE, max_size);
    texture_atlas->max_size = max_size;
    
    texture_atlas->skyline_anchors = (IntVec2Array) {malloc(sizeof(IntVec2) * DEFAULT_SKYLINE_ANCHOR_CAP), 0, DEFAULT_SKYLINE_ANCHOR_CAP};
    ensure(texture_atlas->skyline_anchors.items != NULL);
    append_IntVec2Array(&texture_atlas->skyline_anchors, (IntVec2) {0, 0});
    
    texture_atlas->rects = (TextureRectArray) {malloc(sizeof(TextureRect) * DEFAULT_TEXTURE_RECTS_CAP), 0, DEFAULT_TEXTURE_RECTS_CAP};
    ensure(texture_atlas->rects.items != NULL);
    
    texture_atlas->image = (Image) {
        calloc(1, texture_atlas->size * texture_atlas->size),
        texture_atlas->size, texture_atlas->size,
        1,
        PIXELFORMAT_UNCOMPRESSED_GRAYSCALE
    };
    ensure(texture_atlas->image.data != NULL);
    texture_atlas->image_changed = true;

    texture_atlas->texture = (Texture2D) { .id = 0 }; // texture.id = 0 means UnloadTexture has no effect

    return texture_atlas;
}

void texture_atlas_destroy(TextureAtlas **texture_atlas) {
    if (*texture_atlas == NULL) { return; }
    free((*texture_atlas)->skyline_anchors.items);
    free((*texture_atlas)->rects.items);
    UnloadTexture((*texture_atlas)->texture);
    UnloadImage((*texture_atlas)->image);
    free((*texture_atlas));
    *texture_atlas = NULL;
}

static Anchor find_best_anchor(IntVec2Array *anchors, int size, int max_size, int w) {
    Anchor best = {max_size, max_size, 0};
    for (int i = 0; i < anchors->count; i++) {
        IntVec2 candidate = anchors->items[i];
        if (candidate.x+w > size) { break; }
        for (int j = i; j < anchors->count; j++) {
            IntVec2 neighbor;
            if (j+1 < anchors->count) { neighbor = anchors->items[j+1]; }
            else { neighbor = (IntVec2) {size, 0}; }
            if (w - (neighbor.x - candidate.x) <= 0) {
                if (candidate.y < best.y) { best = (Anchor) {candidate.x, candidate.y, i}; }
                break;
            }
            if (candidate.y < neighbor.y) { candidate.y = neighbor.y; }
            if (candidate.y >= best.y) { break; }
        }
    }
    return best;
}

static void update_anchors(IntVec2Array *anchors, Anchor best, int w, int h) {
    insert_IntVec2Array(anchors, best.index, (IntVec2) {best.x, best.y+h});
    int latest_y;
    do {
        latest_y = anchors->items[best.index + 1].y;
        delete_IntVec2Array(anchors, best.index + 1);
    } while (best.index + 1 < anchors->count && anchors->items[best.index+1].x < best.x + w);
    if (best.index + 1 == anchors->count || anchors->items[best.index+1].x > best.x + w) {
        insert_IntVec2Array(anchors, best.index + 1, (IntVec2) {best.x + w, latest_y});
    }
}

TA_Status texture_atlas_get_rect(TextureRect *return_rect, const TextureAtlas *texture_atlas, uint32_t key) {
    for (int i = 0; i < texture_atlas->rects.count; i++) {
        if (texture_atlas->rects.items[i].key == key) {
            *return_rect = texture_atlas->rects.items[i];
            return TA_OK;
        }
    }
    return TA_RECT_NOT_FOUND;
}

TA_Status texture_atlas_add_get_rect(TextureRect *return_rect, TextureAtlas *texture_atlas, uint32_t key, Image image, int origin_x, int origin_y) {
    /* Uses Skyline packing algorithm = new rectangles go bottom most, left most space available.
    Possible bottom left corner positions for new rectangles are stored in TextureAtlas->anchors and are sorted by x.
    Each anchor is a top left corner of a previously packed rectangle or a bottom left corner of the remaining space.
    Other corners wouldn't be used because they can't be optimal.
    */
    assert(return_rect != NULL);
    TextureRect existing_rect;
    if (texture_atlas_get_rect(&existing_rect, texture_atlas, key) == TA_RECT_NOT_FOUND) {
        IntVec2Array *anchors = &texture_atlas->skyline_anchors; // shorthand
        assert(anchors->count > 0);
        assert(texture_atlas->max_size >= image.width && image.width > 0 && texture_atlas->max_size >= image.height && image.height > 0);
        int old_size = texture_atlas->size;
        while (true) { // # retry loop: repeats if size is expanded. other paths return
            assert(texture_atlas->size <= texture_atlas->max_size);
            Anchor best = find_best_anchor(anchors, texture_atlas->size, texture_atlas->max_size, image.width);
            
            if (best.y+image.height > texture_atlas->size) {
                if (texture_atlas->size == texture_atlas->max_size) { return TA_MAX_SIZE_EXCEEDED; }
                // Anchor adjustments: increasing size can add / change valid anchors at previous right edge.
                if (anchors->count > 1) { // this if statement is optional / for performance
                    if (anchors->items[anchors->count - 1].x == texture_atlas->size) {
                        anchors->items[anchors->count - 1].y = 0;
                    } else if (anchors->items[anchors->count - 1].y != 0) {
                        append_IntVec2Array(anchors, (IntVec2){texture_atlas->size, 0});
                    }
                }
                texture_atlas->size = imin(texture_atlas->max_size, texture_atlas->size * 2);
            } else {
                TextureRect rect = (TextureRect) {key, best.x, best.y, image.width, image.height, origin_x, origin_y};
                append_TextureRectArray(&texture_atlas->rects, rect);
                update_anchors(anchors, best, image.width, image.height);
                
                if (old_size < texture_atlas->size) {
                    ImageResizeCanvas(&texture_atlas->image, texture_atlas->size, texture_atlas->size, 0, 0, (Color) {0, 0, 0, 0});
                }

                ImageDraw(
                    &texture_atlas->image, image,
                    (Rectangle) {0, 0, image.width, image.height},
                    (Rectangle) {rect.x, rect.y, rect.w, rect.h},
                    (Color) {255, 255, 255, 255}
                );

                *return_rect = rect;
                return TA_OK;
            }
        }

    } else { *return_rect = existing_rect; }
    return TA_OK;
}

Texture2D texture_atlas_get_texture(TextureAtlas *texture_atlas) {
    if (texture_atlas->image_changed) {
        assert(IsImageValid(texture_atlas->image));
        UnloadTexture(texture_atlas->texture);
        texture_atlas->texture = LoadTextureFromImage(texture_atlas->image);
        assert(IsTextureValid(texture_atlas->texture));
        SetTextureFilter(texture_atlas->texture, TEXTURE_FILTER_POINT);
        texture_atlas->image_changed = false;
    }
    return texture_atlas->texture;
}

TA_Status texture_atlas_draw(TextureAtlas *texture_atlas, uint32_t key, int x, int y, Color tint) {
    TextureRect rect;
    TA_Status status = texture_atlas_get_rect(&rect, texture_atlas, key);
    if (status == TA_RECT_NOT_FOUND) { return status; }
    Rectangle src = { rect.x, rect.y, rect.w, rect.h };
    Rectangle dest = { x + rect.origin_x, y - rect.origin_y, rect.w, rect.h };
    Texture2D texture = texture_atlas_get_texture(texture_atlas);
    BeginBlendMode(BLEND_ALPHA);
    DrawTexturePro(texture, src, dest, (Vector2) {0, 0}, 0, tint);
    EndBlendMode();
    return TA_OK;
}

// SS_Status texture_atlas_write_bmp(const TextureAtlas *texture_atlas, const char* path) {
//     SS_Status status = bmp_write(path, texture_atlas->image, texture_atlas->size, texture_atlas->size, texture_atlas->channels);
//     if (status.code != 0) return report(1, "Unable to write texture atlas.\n%s", status.message);
//     return status;
// }