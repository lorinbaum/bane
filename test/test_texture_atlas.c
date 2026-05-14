#include <stdlib.h>
#include <stdio.h>
#include "bane.h"

#define RECT_MAX_W 5
#define RECT_MAX_H 5

Image create_img(int w, int h) {
    Image img = (Image) {malloc(w*h), w, h, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
    ensure(img.data != NULL);
    return img;
}

void add_dummy_rect(TextureAtlas *texture_atlas, uint32_t seed, unsigned int count) {
    SetRandomSeed(seed);
    for (unsigned int key = 0; key < count; key++) {
        int w = GetRandomValue(1, RECT_MAX_W), h = GetRandomValue(1, RECT_MAX_H), gray = GetRandomValue(1, 255);
        TextureRect rect;
        Image img = create_img(w, h);
        ImageClearBackground(&img, (Color) {gray, gray, gray, 255});
        TAStatus status = texture_atlas_add_get_rect(&rect, texture_atlas, key, img, 0, 0);
        assert(status == TA_OK);
        UnloadImage(img);
    }
}

void test_skyline_overlap() {
    TextureAtlas *TA = texture_atlas_create(64);
    TextureRect rect, comp_rect;
    add_dummy_rect(TA, 5, 400);
    for (unsigned int i = 0; i < TA->rects.count; i++) {
        rect = TA->rects.items[i];
        assert(rect.x >= 0 && rect.x + rect.w <= TA->size);
        assert(rect.y >= 0 && rect.y + rect.h <= TA->size);
        for (unsigned int j = 0; j < TA->rects.count; j++) {
            if (i == j) { continue; }
            comp_rect = TA->rects.items[j];
            assert(
                comp_rect.x + comp_rect.w <= rect.x ||
                comp_rect.x >= rect.x + rect.w      ||
                comp_rect.y + comp_rect.h <= rect.y ||
                comp_rect.y >= rect.y + rect.h
            );
        }
    }
    texture_atlas_destroy(&TA);
}

void test_golden_skyline() {
    TextureAtlas *TA = texture_atlas_create(128);
    add_dummy_rect(TA, 1, 600);
    const char *golden_path = TEST_DATA_DIR "/golden_skyline.png";
    const char *fail_path = TEST_DATA_DIR "/golden_skyline_FAILED.png";
    if (FileExists(golden_path)) {
        Image golden = LoadImage(golden_path);
        assert(IsImageValid(golden)); // could fail if the file does not exist
        int size = GetPixelDataSize(golden.width, golden.height, golden.format);
        assert(size == GetPixelDataSize(TA->image.width, TA->image.height, TA->image.format));
        uint8_t *ref_data = (uint8_t *) TA->image.data;
        uint8_t *golden_data = (uint8_t *) golden.data;
        for (int i = 0; i < size; i++) {
            if (ref_data[i] != golden_data[i]) {
                ExportImage(TA->image, fail_path);
                printf("FAILED IMAGE IMAGE WRITTEN TO %s\n", fail_path);
                exit(1);
            }
        }
        UnloadImage(golden);
    } else {
        ExportImage(TA->image, fail_path);
        printf("NO REFERENCE IMAGE FOUND AT %s. FAILED IMAGE WRITTEN TO %s\n", golden_path, fail_path);
        exit(1);
    }
    texture_atlas_destroy(&TA);
}

void test_atlas_expand() {
    TextureAtlas *TA = texture_atlas_create(DEFAULT_TEXTURE_ATLAS_SIZE * 2);
    assert(TA->size == DEFAULT_TEXTURE_ATLAS_SIZE);
    const int s = DEFAULT_TEXTURE_ATLAS_SIZE / 2;
    TextureRect rect;
    TAStatus status;
    Image img = create_img(s, s);
    for (int i = 0; i < 16; i++) {
        status = texture_atlas_add_get_rect(&rect, TA, i, img, 0, 0);
        assert(status == TA_OK);
    }
    assert(TA->size == DEFAULT_TEXTURE_ATLAS_SIZE * 2);
    status = texture_atlas_add_get_rect(&rect, TA, 16, img, 0, 0);
    assert(status == TA_MAX_SIZE_EXCEEDED);
    UnloadImage(img);
    texture_atlas_destroy(&TA);
}

void test_overhanging_rect() {
    TextureAtlas *TA = texture_atlas_create(8);
    TextureRect rect;
    TAStatus status;
    Image img;
    
    img = create_img(8, 2);
    status = texture_atlas_add_get_rect(&rect, TA, 0, img, 0, 0);
    assert(status == TA_OK);
    assert(rect.x == 0 && rect.y == 0);
    UnloadImage(img);
    
    img = create_img(4, 2);
    status = texture_atlas_add_get_rect(&rect, TA, 1, img, 0, 0);
    assert(status == TA_OK);
    assert(rect.x == 0 && rect.y == 2);
    UnloadImage(img);

    img = create_img(6, 2);
    status = texture_atlas_add_get_rect(&rect, TA, 2, img, 0, 0);
    assert(status == TA_OK);
    assert(rect.x == 0 && rect.y == 4);
    UnloadImage(img);

    img = create_img(2, 6);
    status = texture_atlas_add_get_rect(&rect, TA, 3, img, 0, 0);
    assert(status == TA_OK);
    assert(rect.x == 6 && rect.y == 2);
    UnloadImage(img);

    texture_atlas_destroy(&TA);
}

void test_same_key() {
    TextureAtlas *TA = texture_atlas_create(8);
    Image img;
    TextureRect rect;
    TAStatus status;

    img = create_img(2, 2);
    status = texture_atlas_add_get_rect(&rect, TA, 0, img, 0, 0);
    assert(status == TA_OK);
    UnloadImage(img);

    img = create_img(4, 4);
    status = texture_atlas_add_get_rect(&rect, TA, 0, img, 0, 0);
    assert(status == TA_OK);
    assert(rect.w == 2 && rect.h == 2);
    assert(TA->rects.count == 1);
    UnloadImage(img);
    
    texture_atlas_destroy(&TA);
}

int main() {
    test_skyline_overlap();
    test_atlas_expand();
    test_overhanging_rect();
    test_golden_skyline();
    test_same_key();
    return 0;
}
