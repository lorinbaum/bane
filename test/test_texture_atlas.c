#include <stdlib.h>
#include <stdio.h>
#include "bane.h"

#define RECT_MAX_W 5
#define RECT_MAX_H 5

#define RAND_DIMS 128u
#define RAND_GRAYS 256u
// for reproducible golden test, values 1-5
uint_least8_t rand_dims[RAND_DIMS] = {
    5, 4, 2, 5, 1, 5, 4, 2, 1, 3, 4, 3, 5, 3, 2, 4, 5, 2, 4, 3, 2, 1, 2, 3, 3, 5, 1, 4, 4, 1, 4, 4,
    3, 3, 3, 4, 2, 2, 2, 3, 5, 3, 3, 3, 5, 1, 3, 4, 4, 1, 1, 3, 4, 1, 3, 3, 3, 5, 4, 4, 3, 2, 1, 1,
    1, 2, 1, 4, 5, 2, 2, 2, 4, 1, 4, 3, 4, 2, 5, 5, 5, 1, 4, 2, 1, 1, 5, 4, 3, 2, 4, 1, 3, 2, 1, 1,
    2, 4, 4, 1, 4, 5, 3, 3, 1, 4, 5, 1, 2, 2, 3, 5, 3, 2, 1, 3, 4, 5, 1, 5, 5, 1, 3, 1, 2, 4, 1, 2
};

// shuffled unique gray values for golden test 0 - 255
uint_least8_t rand_grays[RAND_GRAYS] = {
    46 ,  23,  11, 155, 230, 236, 233,   1,  12,  86,   5, 126, 175, 150, 227, 253,  94, 208, 157,  70, 165, 185, 149, 100, 153, 187,  85,  39, 218,  28, 152,  20,
    148,  50, 160, 141,  62,  76, 248, 210, 224, 112,  71, 102, 191,  59,  87, 129, 120, 114,  13, 154, 135, 180,  75, 137, 107, 246, 136,  58,  10, 119, 176, 212,
    173,  17, 167, 231, 215, 243, 146, 163, 195,  18, 226,  79, 251,  77, 174, 139,  27,  96, 239, 209,   8,  37, 184, 159, 213, 147, 122, 229, 240, 151, 197,  47,
    138, 123,  26,  84, 179, 161, 189, 199,  63,   0,  69,  65,  51, 108,  21, 200,  34, 222, 250, 202, 116,  38,  61, 203, 211,  49, 205,  93,  74, 162, 134,   6,
    56,  121,  52, 142,  95, 255, 204, 110, 188,  16,  30,  92,  55, 225, 206,  32, 168,  80, 103,  31,  73, 140,   3, 245, 223, 125, 128, 247, 228, 214,  22,  82,
    219,  45, 201,  33,  24,  97, 252, 220, 217, 132,  53, 127, 124, 254, 143,  29,  35,  98,  25,  42, 238,  14, 133, 145, 111, 241, 106, 164, 169, 181, 104, 234,
    183, 198,  83, 166, 172,  44, 249,  60, 170,  91, 113, 216, 115, 193, 196, 144,  99, 105, 118, 190,  41, 109, 117, 182, 177,  68,  40,  15, 235,  81, 221, 194,
    19,  156, 101,   4,   2, 131, 192,  36, 242, 244,  48, 232,  72,  67,  43, 237,  57, 130,  88,  89, 178,  78,  66,   7, 207, 171,   9, 158, 186,  64,  54,  90,
};

Image create_img(int w, int h) {
    Image img = {malloc(w*h), w, h, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
    ensure(img.data != NULL);
    return img;
}

void add_dummy_rect(TextureAtlas *texture_atlas, unsigned int count) {
    for (unsigned int key = 0; key < count; key++) {
        uint_least8_t w = rand_dims[key % RAND_DIMS], h = rand_dims[(key + 1) % RAND_DIMS];
        uint_least8_t gray = rand_grays[key % RAND_GRAYS];
        TextureRect rect;
        Image img = create_img(w, h);
        ImageClearBackground(&img, (Color) {gray, gray, gray, 255});
        TaError error = texture_atlas_add_get_rect(texture_atlas, key, img, 0, 0, &rect);
        assert(!error);
        UnloadImage(img);
    }
}

void test_skyline_overlap() {
    TextureAtlas TA = texture_atlas_create(64);
    TaError error;
    TextureRect rect, comp_rect;
    unsigned int count = 400;
    add_dummy_rect(&TA, count);
    for (unsigned int i = 0; i < count; i++) {
        error = texture_atlas_get_rect(TA, i, &rect);
        assert(!error);
        assert(rect.x >= 0 && rect.x + rect.w <= TA.size);
        assert(rect.y >= 0 && rect.y + rect.h <= TA.size);
        for (unsigned int j = 0; j < count; j++) {
            if (i == j) { continue; }
            error = texture_atlas_get_rect(TA, j, &comp_rect);
            assert(!error);
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
    TextureAtlas TA = texture_atlas_create(128);
    add_dummy_rect(&TA, 800);
    const char *golden_path = TEST_DATA_DIR "/golden_skyline.png";
    const char *fail_path = TEST_DATA_DIR "/golden_skyline_FAILED.png";
    if (FileExists(golden_path)) {
        Image golden = LoadImage(golden_path);
        assert(IsImageValid(golden)); // could fail if the file does not exist
        int size = GetPixelDataSize(golden.width, golden.height, golden.format);
        assert(size == GetPixelDataSize(TA.image.width, TA.image.height, TA.image.format));
        uint8_t *ref_data = (uint8_t *) TA.image.data;
        uint8_t *golden_data = (uint8_t *) golden.data;
        for (int i = 0; i < size; i++) {
            if (ref_data[i] != golden_data[i]) {
                ExportImage(TA.image, fail_path);
                printf("FAILED IMAGE IMAGE WRITTEN TO %s\n", fail_path);
                exit(1);
            }
        }
        UnloadImage(golden);
    } else {
        ExportImage(TA.image, fail_path);
        printf("NO REFERENCE IMAGE FOUND AT %s. FAILED IMAGE WRITTEN TO %s\n", golden_path, fail_path);
        exit(1);
    }
    texture_atlas_destroy(&TA);
}

void test_atlas_expand() {
    TextureAtlas TA = texture_atlas_create(DEFAULT_TEXTURE_ATLAS_SIZE * 2);
    assert(TA.size == DEFAULT_TEXTURE_ATLAS_SIZE);
    const int s = DEFAULT_TEXTURE_ATLAS_SIZE / 2;
    TextureRect rect;
    TaError error;
    Image img = create_img(s, s);
    for (int i = 0; i < 16; i++) {
        error = texture_atlas_add_get_rect(&TA, i, img, 0, 0, &rect);
        assert(!error);
    }
    assert(TA.size == DEFAULT_TEXTURE_ATLAS_SIZE * 2);
    error = texture_atlas_add_get_rect(&TA, 16, img, 0, 0, &rect);
    assert(error == TA_MAX_SIZE_EXCEEDED);
    UnloadImage(img);
    texture_atlas_destroy(&TA);
}

void test_overhanging_rect() {
    TextureAtlas TA = texture_atlas_create(8);
    TextureRect rect;
    TaError error;
    Image img;
    
    img = create_img(8, 2);
    error = texture_atlas_add_get_rect(&TA, 0, img, 0, 0, &rect);
    assert(!error);
    assert(rect.x == 0 && rect.y == 0);
    UnloadImage(img);
    
    img = create_img(4, 2);
    error = texture_atlas_add_get_rect(&TA, 1, img, 0, 0, &rect);
    assert(!error);
    assert(rect.x == 0 && rect.y == 2);
    UnloadImage(img);

    img = create_img(6, 2);
    error = texture_atlas_add_get_rect(&TA, 2, img, 0, 0, &rect);
    assert(!error);
    assert(rect.x == 0 && rect.y == 4);
    UnloadImage(img);

    img = create_img(2, 6);
    error = texture_atlas_add_get_rect(&TA, 3, img, 0, 0, &rect);
    assert(!error);
    assert(rect.x == 6 && rect.y == 2);
    UnloadImage(img);

    texture_atlas_destroy(&TA);
}

void test_same_key() {
    TextureAtlas TA = texture_atlas_create(8);
    Image img;
    TextureRect rect;
    TaError error;

    img = create_img(2, 2);
    error = texture_atlas_add_get_rect(&TA, 0, img, 0, 0, &rect);
    assert(!error);
    UnloadImage(img);

    img = create_img(4, 4);
    error = texture_atlas_add_get_rect(&TA, 0, img, 0, 0, &rect);
    assert(!error);
    assert(rect.w == 2 && rect.h == 2);
    UnloadImage(img);
    
    texture_atlas_destroy(&TA);
}

void test_rectmap() {
    RectMap map;
    RmError error;
    error = rectmap_create(5, &map);
    TextureRect rect;
    TextureRect rect_stored;
    for (uint16_t i = 0; i < UINT16_MAX - 1; i++) {
        rect = (TextureRect) { .key = i, .x = i + 5 };
        error = rectmap_put(&map, rect);
        assert(!error);
        error = rectmap_get(map, i, &rect_stored);
        assert(!error);
        assert(map.used == i + 1);
        assert(map.max_entries >= i + 1);
        assert(rect_stored.key == rect.key && rect_stored.x == rect.x);
    }

    // recheck all values because map has been resized since last checking the first ones
    for (uint32_t i = 0; i < 128; i++) {
        error = rectmap_get(map, i, &rect_stored);
        assert(!error);
        assert(rect_stored.key == i && rect_stored.x == i + 5);
    }

    // check size boundary
    rect = (TextureRect) { .key = UINT16_MAX }; // UINT16_MAX here is just an unused key. can be whatever
    error = rectmap_put(&map, rect);
    assert(error == RM_MAX_SIZE_REACHED);

    // check value update
    uint32_t key = 500;
    error = rectmap_get(map, key, &rect_stored);
    assert(!error);
    rect = rect_stored;
    rect.x += 200;
    error = rectmap_put(&map, rect);
    assert(!error);
    error = rectmap_get(map, key, &rect_stored);
    assert(!error);
    assert(rect_stored.x == rect.x);

    rectmap_destroy(&map);
}

int main() {
    test_skyline_overlap();
    test_atlas_expand();
    test_overhanging_rect();
    test_golden_skyline();
    test_same_key();
    test_rectmap();
    return 0;
}
