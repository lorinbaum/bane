#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "bane.h"

TextureAtlas *texture_atlas_create(int max_size) {
    assert(max_size > 0);
    TextureAtlas *texture_atlas = calloc(1, sizeof(TextureAtlas));
    ensure(texture_atlas != NULL);
    texture_atlas->size = imin(DEFAULT_TEXTURE_ATLAS_SIZE, max_size);
    texture_atlas->max_size = max_size;
    
    texture_atlas->skyline_anchors = (IntVec2Array) {malloc(sizeof(IntVec2) * DEFAULT_SKYLINE_ANCHOR_CAP), 0, DEFAULT_SKYLINE_ANCHOR_CAP};
    ensure(texture_atlas->skyline_anchors.items != NULL);
    append_IntVec2Array(&texture_atlas->skyline_anchors, (IntVec2) {0, 0});
    append_IntVec2Array(&texture_atlas->skyline_anchors, (IntVec2) {texture_atlas->size, 0}); // sentinel
    
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

    texture_atlas->texture = (Texture2D) { 0 }; // texture.id = 0 means UnloadTexture has no effect

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

TAStatus texture_atlas_get_rect(TextureRect *return_rect, const TextureAtlas *texture_atlas, uint32_t key) {
    for (unsigned int i = 0; i < texture_atlas->rects.count; i++) {
        if (texture_atlas->rects.items[i].key == key) {
            *return_rect = texture_atlas->rects.items[i];
            return TA_OK;
        }
    }
    return TA_RECT_NOT_FOUND;
}

typedef struct { int x, y, index; } Anchor;

static bool find_best_anchor(IntVec2Array *anchors, Anchor *ret, int size, int max_size, int w) {
    Anchor best = {max_size, max_size, 0};
    IntVec2 candidate, neighbor;
    for (unsigned int i = 0; i < anchors->count - 1; i++) {
        candidate = anchors->items[i];
        if (candidate.x+w > size) { break; }
        for (unsigned int j = i+1; j < anchors->count; j++) {
            neighbor = anchors->items[j];
            if (w - (neighbor.x - candidate.x) <= 0) {
                if (candidate.y < best.y) { best = (Anchor) {candidate.x, candidate.y, i}; }
                break;
            }
            if (candidate.y < neighbor.y) { candidate.y = neighbor.y; }
            if (candidate.y >= best.y) { break; }
        }
    }
    bool found = best.x < max_size && best.y < max_size;
    if (found) { *ret = best; }
    return found;
}

static void update_anchors(IntVec2Array *anchors, Anchor best, int w, int h) {
    int latest_y;
    unsigned int next_i = best.index;
    if (anchors->items[best.index].y != best.y+h) { // anchors to the left and at same y would always be preferred anyway
        insert_IntVec2Array(anchors, best.index, (IntVec2) {best.x, best.y+h});
        next_i++;
    }
    do {
        latest_y = anchors->items[next_i].y;
        delete_IntVec2Array(anchors, next_i);
    } while (next_i + 1 < anchors->count && anchors->items[best.index+1].x < best.x + w); // next_i + 1 because last anchor is sentinel and mustn't be deleted.
    if (anchors->items[best.index+1].x > best.x + w) { insert_IntVec2Array(anchors, next_i, (IntVec2) {best.x + w, latest_y}); }
}

static TAStatus texture_atlas_resize(int *size, int max_size, IntVec2Array *anchors) {
    if (*size == max_size) { return TA_MAX_SIZE_EXCEEDED; }
    *size = imin(max_size, *size * 2);
    // update or add new old sentinel
    if (anchors->items[anchors->count-2].y != 0) { append_IntVec2Array(anchors, (IntVec2) {*size, 0}); }
    else { anchors->items[anchors->count - 1].x = *size; }
    return TA_OK;
}


TAStatus texture_atlas_add_get_rect(TextureRect *return_rect, TextureAtlas *texture_atlas, uint32_t key, Image image, int origin_x, int origin_y) {
    /* Uses Skyline packing algorithm
    Assuming coordinate origin in top left corner and Y increasing downwards:
    New rectangles go to top most, left most space available.
    Possible top left corner positions for new rectangles are stored in TextureAtlas->anchors and are sorted by x.
    Each anchor is a top left corner of the remaining space, except the last anchor (sentinel), which is the top right corner of the atlas.
    Other positions wouldn't be used given the algorithm.
    Each TextureRect.x and .y refer to the top left corner of the associated image in the texture atlas.
    */
    assert(return_rect != NULL && texture_atlas != NULL);
    TextureRect rect;
    TAStatus status = texture_atlas_get_rect(&rect, texture_atlas, key);
    if (status == TA_RECT_NOT_FOUND) {
        Anchor best;
        IntVec2Array *anchors = &texture_atlas->skyline_anchors; // shorthand
        assert(anchors->count >= 2);
        assert(texture_atlas->max_size >= image.width && image.width > 0 && texture_atlas->max_size >= image.height && image.height > 0);
        bool found;
        int old_size = texture_atlas->size;
        while (true) { // retry after resize
            assert(texture_atlas->size <= texture_atlas->max_size);
            found = find_best_anchor(anchors, &best, texture_atlas->size, texture_atlas->max_size, image.width);
            if (!found || best.y+image.height > texture_atlas->size) {
                status = texture_atlas_resize(&texture_atlas->size, texture_atlas->max_size, anchors);
                if (status != TA_OK) { return status; }
            } else {
                rect = (TextureRect) {key, best.x, best.y, image.width, image.height, origin_x, origin_y};
                append_TextureRectArray(&texture_atlas->rects, rect);
                update_anchors(anchors, best, image.width, image.height);
                if (old_size < texture_atlas->size) {
                    ImageResizeCanvas(&texture_atlas->image, texture_atlas->size, texture_atlas->size, 0, 0, (Color) {0, 0, 0, 0});
                }
                ImageDraw(&texture_atlas->image, image,
                    (Rectangle) {0, 0, image.width, image.height}, (Rectangle) {rect.x, rect.y, rect.w, rect.h},
                    (Color) {255, 255, 255, 255}
                );
                texture_atlas->image_changed = true;
                status = TA_OK;
                break;
            }
        }
    }
    *return_rect = rect;
    return status;
}

void texture_atlas_update_texture(TextureAtlas *texture_atlas) {
    assert(IsImageValid(texture_atlas->image));
    if (
        texture_atlas->image.width == texture_atlas->texture.width &&
        texture_atlas->image.height == texture_atlas->texture.height &&
        texture_atlas->image.format == texture_atlas->texture.format
    ) { UpdateTexture(texture_atlas->texture, texture_atlas->image.data);
    } else {
        UnloadTexture(texture_atlas->texture);
        texture_atlas->texture = LoadTextureFromImage(texture_atlas->image);
        assert(IsTextureValid(texture_atlas->texture));
        SetTextureFilter(texture_atlas->texture, TEXTURE_FILTER_POINT);
    }
}

TAStatus texture_atlas_draw(TextureAtlas *texture_atlas, uint32_t key, int x, int y, Color tint) {
    TextureRect rect;
    TAStatus status = texture_atlas_get_rect(&rect, texture_atlas, key);
    if (status == TA_RECT_NOT_FOUND) { return status; }
    Rectangle src = { rect.x, rect.y, rect.w, rect.h };
    Rectangle dest = { x + rect.origin_x, y - rect.origin_y, rect.w, rect.h };
    if (texture_atlas->image_changed) {
        texture_atlas_update_texture(texture_atlas);
        texture_atlas->image_changed = false;
    }
    DrawTexturePro(texture_atlas->texture, src, dest, (Vector2) {0, 0}, 0, tint);
    return TA_OK;
}
