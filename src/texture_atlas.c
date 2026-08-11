#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "bane.h"

static uint32_t murmur3_finalize(uint32_t x) {
    x ^= x >> 16;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return x;
}

RmError rectmap_create(uint16_t max_entries, RectMap *ret) {
    if (max_entries == UINT16_MAX ) { return RM_MAX_SIZE_REACHED; }
    uint32_t bucket_count = (uint32_t) (max_entries / RECTMAP_MAX_LOADFACTOR + 1);
    RectMap map = (RectMap) {
        .max_entries = max_entries,
        .used = 0,
        .entries = malloc(max_entries * sizeof(TextureRect)),
        .bucket_count = bucket_count,
        .buckets = calloc(1, bucket_count * sizeof(uint16_t)) 
    };
    ensure(map.entries != NULL && map.buckets != NULL);
    *ret = map;
    return RM_OK;
}

RmError rectmap_get(RectMap map, uint32_t key, TextureRect *ret) {
    uint32_t hash = murmur3_finalize(key) % map.bucket_count;
    uint16_t offset;
    TextureRect entry;
    while (true) {
        offset = map.buckets[hash];
        if (offset == 0) { return RM_KEY_NOT_FOUND; }
        entry = map.entries[offset - 1];
        if (entry.key == key) {
            *ret = entry;
            return RM_OK;
        }
        hash = (hash + 1) % map.bucket_count;
    }
}

static void rectmap_enlarge(RectMap *map) {
    map->max_entries = min(UINT16_MAX - 1, map->max_entries * 2);
    map->bucket_count = (uint32_t) (map->max_entries / RECTMAP_MAX_LOADFACTOR + 1);
    void *temp = realloc(map->entries, map->max_entries * sizeof(TextureRect));
    ensure(temp != NULL);
    map->entries = temp;
    temp = calloc(1, map->bucket_count * sizeof(uint16_t));
    ensure(temp != NULL);
    free(map->buckets);
    map->buckets = temp;
    uint32_t hash;
    uint16_t offset;
    for (uint16_t i = 0; i < map->used; i++) {
        hash = murmur3_finalize(map->entries[i].key) % map->bucket_count;
        while (true) {
            offset = map->buckets[hash];
            if (offset == 0) {
                map->buckets[hash] = i + 1;
                break;
            }
            hash = (hash + 1) % map->bucket_count;
        }
    }
}

RmError rectmap_put(RectMap *map, TextureRect rect) {
    uint16_t offset;
    TextureRect entry;
    uint32_t hash = murmur3_finalize(rect.key) % map->bucket_count;
    while (true) {
        offset = map->buckets[hash];
        if (offset == 0) { // add new value
            if (map->used + 1 == UINT16_MAX) { return RM_MAX_SIZE_REACHED; }
            if (map->used + 1 > map->max_entries) {
                rectmap_enlarge(map);
                hash = murmur3_finalize(rect.key) % map->bucket_count;
                continue;
            }
            map->buckets[hash] = map->used + 1; // stores one higher because offest = 0 is reserved for unused fields
            map->entries[map->used] = rect;
            map->used++;
            return RM_OK;
        } else { // replace value
            entry = map->entries[offset - 1];
            if (entry.key == rect.key) {
                map->entries[offset - 1] = rect;
                return RM_OK;
            }
        }
        hash = (hash + 1) % map->bucket_count;
    }
}

void rectmap_destroy(RectMap *map) {
    free(map->buckets);
    map->buckets = NULL;
    free(map->entries);
    map->entries = NULL;
}

TextureAtlas texture_atlas_create(int max_size) {
    assert(max_size > 0);
    TextureAtlas texture_atlas = {
        .size = min(DEFAULT_TEXTURE_ATLAS_SIZE, max_size),
        .max_size = max_size,
        .skyline_anchors = sb_create(IntVec2, DEFAULT_SKYLINE_ANCHOR_CAP),
        .image = (Image) {
            .data = calloc(1, texture_atlas.size * texture_atlas.size),
            .width = texture_atlas.size,
            .height = texture_atlas.size,
            .format = IMAGE_FORMAT_ALPHA
        }
    };
    ensure(texture_atlas.image.data != NULL);
    sb_append(&texture_atlas.skyline_anchors, (IntVec2) {0, 0});
    sb_append(&texture_atlas.skyline_anchors, (IntVec2) {texture_atlas.size, 0}); // sentinel
    RmError error = rectmap_create(256, &texture_atlas.rects);
    ensure(!error);
    return texture_atlas;
}

void texture_atlas_destroy(TextureAtlas *texture_atlas) {
    sb_destroy(&texture_atlas->skyline_anchors);
    rectmap_destroy(&texture_atlas->rects);
    free(texture_atlas->image.data);
    memset(texture_atlas, 0, sizeof(TextureAtlas));
}

TaError texture_atlas_get_rect(TextureAtlas texture_atlas, uint32_t key, TextureRect *return_rect) {
    return rectmap_get(texture_atlas.rects, key, return_rect) ? TA_RECT_NOT_FOUND : TA_OK;
}

typedef struct Anchor { int x, y, index; } Anchor;

static bool find_best_anchor(sb_IntVec2 *anchors, int size, int max_size, int w, Anchor *ret) {
    Anchor best = {max_size, max_size, 0};
    IntVec2 candidate, neighbor;
    for (unsigned int i = 0; i < anchors->count - 1; i++) {
        candidate = sb_get(*anchors, i);
        if (candidate.x+w > size) { break; }
        for (unsigned int j = i+1; j < anchors->count; j++) {
            neighbor = sb_get(*anchors, j);
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

static void update_anchors(sb_IntVec2 *anchors, Anchor best, int w, int h) {
    assert(anchors != NULL);
    int latest_y;
    unsigned int next_i = best.index;
    if (sb_get(*anchors, best.index).y != best.y+h) { // anchors to the left and at same y would always be preferred anyway
        sb_insert(anchors, best.index, (IntVec2) {best.x, best.y+h});
        next_i++;
    }
    do {
        latest_y = sb_get(*anchors, next_i).y;
        sb_delete(anchors, next_i);
    } while (next_i + 1 < anchors->count && sb_get(*anchors, best.index + 1).x < best.x + w); // next_i + 1 because last anchor is sentinel and mustn't be deleted.
    if (sb_get(*anchors, best.index + 1).x > best.x + w) { sb_insert(anchors, next_i, (IntVec2) {best.x + w, latest_y}); }
}

static TaError texture_atlas_resize(size_t *size, size_t max_size, sb_IntVec2 *anchors) {
    assert(size != NULL && anchors != NULL);
    if (*size == max_size) { return TA_MAX_SIZE_EXCEEDED; }
    *size = min(max_size, *size * 2);
    // update or add new old sentinel
    if (sb_rget(*anchors, 2).y != 0) { sb_append(anchors, (IntVec2) {*size, 0}); }
    else { sb_rset_attr(*anchors, 1, x, *size); }
    return TA_OK;
}


TaError texture_atlas_add_get_rect(TextureAtlas *texture_atlas, uint32_t key, Image image, int origin_x, int origin_y, TextureRect *return_rect) {
    /* Uses Skyline packing algorithm
    Assuming coordinate origin in top left corner and Y increasing downwards:
    New rectangles go to top most, left most space available.
    Possible top left corner positions for new rectangles are stored in TextureAtlas->anchors and are sorted by x.
    Each anchor is a top left corner of the remaining space, except the last anchor (sentinel), which is the top right corner of the atlas.
    Other positions wouldn't be used given the algorithm.
    Each TextureRect.x and .y refer to the top left corner of the associated image in the texture atlas.
    */
    TextureRect rect;
    TaError error = texture_atlas_get_rect(*texture_atlas, key, &rect);
    if (error == TA_RECT_NOT_FOUND) {
        if (image.width == 0 || image.height == 0) {
            rect = (TextureRect) {key, 0, 0, 0, 0, origin_x, origin_y};
            rectmap_put(&texture_atlas->rects, rect);
        } else {
            Anchor best;
            sb_IntVec2 *anchors = &texture_atlas->skyline_anchors; // shorthand
            assert(anchors->count >= 2);
            assert(texture_atlas->max_size >= image.width && image.width > 0 && texture_atlas->max_size >= image.height && image.height > 0);
            bool found;
            size_t old_size = texture_atlas->size;
            while (true) { // retry after resize
                assert(texture_atlas->size <= texture_atlas->max_size);
                found = find_best_anchor(anchors, texture_atlas->size, texture_atlas->max_size, image.width, &best);
                if (!found || best.y+image.height > texture_atlas->size) {
                    error = texture_atlas_resize(&texture_atlas->size, texture_atlas->max_size, anchors);
                    if (error) return error;
                } else {
                    rect = (TextureRect) {key, best.x, best.y, image.width, image.height, origin_x, origin_y};
                    rectmap_put(&texture_atlas->rects, rect);
                    update_anchors(anchors, best, image.width, image.height);
                    if (old_size < texture_atlas->size) { 
                        image_resize(&texture_atlas->image, texture_atlas->size, texture_atlas->size, 0, 0, (Color) {0, 0, 0, 255});
                    }
                    image_draw(texture_atlas->image, image,
                        (Rectangle) {rect.x, rect.y, rect.w, rect.h}, (Rectangle) {0, 0, image.width, image.height},
                        (Color) {255, 255, 255, 255}
                    );
                    break;
                }
            }  
        }
        error = TA_OK;
    }
    *return_rect = rect;
    return error;
}

TaError texture_atlas_draw(Image dest, Rectangle text_rect, TextureAtlas *texture_atlas, uint32_t key, int x, int y, Color tint) {
    TextureRect rect;
    TaError error = texture_atlas_get_rect(*texture_atlas, key, &rect);
    if (error == TA_RECT_NOT_FOUND) { return error; }
    if (rect.w == 0 || rect.h == 0) { return TA_OK; } // nothing to draw
    Rectangle src_rect = { rect.x, rect.y, rect.w, rect.h };
    Rectangle dest_rect = { x + rect.origin_x, y - rect.origin_y, rect.w, rect.h };
    // printf("textbox: %i,%i, %ix%i\n", text_rect.x, text_rect.y, text_rect.width, text_rect.height);
    // printf("destrect: %i,%i, %ix%i\n", dest_rect.x, dest_rect.y, dest_rect.width, dest_rect.height);
    rect_fit_box(text_rect, &dest_rect, &src_rect); // don't draw outside text box
    // printf("destrect new: %i,%i, %ix%i\n", dest_rect.x, dest_rect.y, dest_rect.width, dest_rect.height);
    // printf("\n");
    image_draw(dest, texture_atlas->image, dest_rect, src_rect, tint);
    return TA_OK;
}
