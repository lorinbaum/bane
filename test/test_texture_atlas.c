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

// HACK!
SDL_Color grayscale_ramp[256] = {
    {  0,   0,   0, 255}, {  1,   1,   1, 255}, {  2,   2,   2, 255}, {  3,   3,   3, 255}, {  4,   4,   4, 255}, {  5,   5,   5, 255}, {  6,   6,   6, 255}, {  7,   7,   7, 255},
    {  8,   8,   8, 255}, {  9,   9,   9, 255}, { 10,  10,  10, 255}, { 11,  11,  11, 255}, { 12,  12,  12, 255}, { 13,  13,  13, 255}, { 14,  14,  14, 255}, { 15,  15,  15, 255},
    { 16,  16,  16, 255}, { 17,  17,  17, 255}, { 18,  18,  18, 255}, { 19,  19,  19, 255}, { 20,  20,  20, 255}, { 21,  21,  21, 255}, { 22,  22,  22, 255}, { 23,  23,  23, 255},
    { 24,  24,  24, 255}, { 25,  25,  25, 255}, { 26,  26,  26, 255}, { 27,  27,  27, 255}, { 28,  28,  28, 255}, { 29,  29,  29, 255}, { 30,  30,  30, 255}, { 31,  31,  31, 255},
    { 32,  32,  32, 255}, { 33,  33,  33, 255}, { 34,  34,  34, 255}, { 35,  35,  35, 255}, { 36,  36,  36, 255}, { 37,  37,  37, 255}, { 38,  38,  38, 255}, { 39,  39,  39, 255},
    { 40,  40,  40, 255}, { 41,  41,  41, 255}, { 42,  42,  42, 255}, { 43,  43,  43, 255}, { 44,  44,  44, 255}, { 45,  45,  45, 255}, { 46,  46,  46, 255}, { 47,  47,  47, 255},
    { 48,  48,  48, 255}, { 49,  49,  49, 255}, { 50,  50,  50, 255}, { 51,  51,  51, 255}, { 52,  52,  52, 255}, { 53,  53,  53, 255}, { 54,  54,  54, 255}, { 55,  55,  55, 255},
    { 56,  56,  56, 255}, { 57,  57,  57, 255}, { 58,  58,  58, 255}, { 59,  59,  59, 255}, { 60,  60,  60, 255}, { 61,  61,  61, 255}, { 62,  62,  62, 255}, { 63,  63,  63, 255},
    { 64,  64,  64, 255}, { 65,  65,  65, 255}, { 66,  66,  66, 255}, { 67,  67,  67, 255}, { 68,  68,  68, 255}, { 69,  69,  69, 255}, { 70,  70,  70, 255}, { 71,  71,  71, 255},
    { 72,  72,  72, 255}, { 73,  73,  73, 255}, { 74,  74,  74, 255}, { 75,  75,  75, 255}, { 76,  76,  76, 255}, { 77,  77,  77, 255}, { 78,  78,  78, 255}, { 79,  79,  79, 255},
    { 80,  80,  80, 255}, { 81,  81,  81, 255}, { 82,  82,  82, 255}, { 83,  83,  83, 255}, { 84,  84,  84, 255}, { 85,  85,  85, 255}, { 86,  86,  86, 255}, { 87,  87,  87, 255},
    { 88,  88,  88, 255}, { 89,  89,  89, 255}, { 90,  90,  90, 255}, { 91,  91,  91, 255}, { 92,  92,  92, 255}, { 93,  93,  93, 255}, { 94,  94,  94, 255}, { 95,  95,  95, 255},
    { 96,  96,  96, 255}, { 97,  97,  97, 255}, { 98,  98,  98, 255}, { 99,  99,  99, 255}, {100, 100, 100, 255}, {101, 101, 101, 255}, {102, 102, 102, 255}, {103, 103, 103, 255},
    {104, 104, 104, 255}, {105, 105, 105, 255}, {106, 106, 106, 255}, {107, 107, 107, 255}, {108, 108, 108, 255}, {109, 109, 109, 255}, {110, 110, 110, 255}, {111, 111, 111, 255},
    {112, 112, 112, 255}, {113, 113, 113, 255}, {114, 114, 114, 255}, {115, 115, 115, 255}, {116, 116, 116, 255}, {117, 117, 117, 255}, {118, 118, 118, 255}, {119, 119, 119, 255},
    {120, 120, 120, 255}, {121, 121, 121, 255}, {122, 122, 122, 255}, {123, 123, 123, 255}, {124, 124, 124, 255}, {125, 125, 125, 255}, {126, 126, 126, 255}, {127, 127, 127, 255},
    {128, 128, 128, 255}, {129, 129, 129, 255}, {130, 130, 130, 255}, {131, 131, 131, 255}, {132, 132, 132, 255}, {133, 133, 133, 255}, {134, 134, 134, 255}, {135, 135, 135, 255},
    {136, 136, 136, 255}, {137, 137, 137, 255}, {138, 138, 138, 255}, {139, 139, 139, 255}, {140, 140, 140, 255}, {141, 141, 141, 255}, {142, 142, 142, 255}, {143, 143, 143, 255},
    {144, 144, 144, 255}, {145, 145, 145, 255}, {146, 146, 146, 255}, {147, 147, 147, 255}, {148, 148, 148, 255}, {149, 149, 149, 255}, {150, 150, 150, 255}, {151, 151, 151, 255},
    {152, 152, 152, 255}, {153, 153, 153, 255}, {154, 154, 154, 255}, {155, 155, 155, 255}, {156, 156, 156, 255}, {157, 157, 157, 255}, {158, 158, 158, 255}, {159, 159, 159, 255},
    {160, 160, 160, 255}, {161, 161, 161, 255}, {162, 162, 162, 255}, {163, 163, 163, 255}, {164, 164, 164, 255}, {165, 165, 165, 255}, {166, 166, 166, 255}, {167, 167, 167, 255},
    {168, 168, 168, 255}, {169, 169, 169, 255}, {170, 170, 170, 255}, {171, 171, 171, 255}, {172, 172, 172, 255}, {173, 173, 173, 255}, {174, 174, 174, 255}, {175, 175, 175, 255},
    {176, 176, 176, 255}, {177, 177, 177, 255}, {178, 178, 178, 255}, {179, 179, 179, 255}, {180, 180, 180, 255}, {181, 181, 181, 255}, {182, 182, 182, 255}, {183, 183, 183, 255},
    {184, 184, 184, 255}, {185, 185, 185, 255}, {186, 186, 186, 255}, {187, 187, 187, 255}, {188, 188, 188, 255}, {189, 189, 189, 255}, {190, 190, 190, 255}, {191, 191, 191, 255},
    {192, 192, 192, 255}, {193, 193, 193, 255}, {194, 194, 194, 255}, {195, 195, 195, 255}, {196, 196, 196, 255}, {197, 197, 197, 255}, {198, 198, 198, 255}, {199, 199, 199, 255},
    {200, 200, 200, 255}, {201, 201, 201, 255}, {202, 202, 202, 255}, {203, 203, 203, 255}, {204, 204, 204, 255}, {205, 205, 205, 255}, {206, 206, 206, 255}, {207, 207, 207, 255},
    {208, 208, 208, 255}, {209, 209, 209, 255}, {210, 210, 210, 255}, {211, 211, 211, 255}, {212, 212, 212, 255}, {213, 213, 213, 255}, {214, 214, 214, 255}, {215, 215, 215, 255},
    {216, 216, 216, 255}, {217, 217, 217, 255}, {218, 218, 218, 255}, {219, 219, 219, 255}, {220, 220, 220, 255}, {221, 221, 221, 255}, {222, 222, 222, 255}, {223, 223, 223, 255},
    {224, 224, 224, 255}, {225, 225, 225, 255}, {226, 226, 226, 255}, {227, 227, 227, 255}, {228, 228, 228, 255}, {229, 229, 229, 255}, {230, 230, 230, 255}, {231, 231, 231, 255},
    {232, 232, 232, 255}, {233, 233, 233, 255}, {234, 234, 234, 255}, {235, 235, 235, 255}, {236, 236, 236, 255}, {237, 237, 237, 255}, {238, 238, 238, 255}, {239, 239, 239, 255},
    {240, 240, 240, 255}, {241, 241, 241, 255}, {242, 242, 242, 255}, {243, 243, 243, 255}, {244, 244, 244, 255}, {245, 245, 245, 255}, {246, 246, 246, 255}, {247, 247, 247, 255},
    {248, 248, 248, 255}, {249, 249, 249, 255}, {250, 250, 250, 255}, {251, 251, 251, 255}, {252, 252, 252, 255}, {253, 253, 253, 255}, {254, 254, 254, 255}, {255, 255, 255, 255}
};

Image create_img(int w, int h) {
    Image img = {.data = malloc(w * h), .format = IMAGE_FORMAT_ALPHA, .width = w, .height =h};
    ensure(img.data != NULL);
    return img;
}

void add_dummy_rect(TextureAtlas *texture_atlas, unsigned int count) {
    for (unsigned int key = 0; key < count; key++) {
        uint_least8_t w = rand_dims[key % RAND_DIMS], h = rand_dims[(key + 1) % RAND_DIMS];
        uint_least8_t gray = rand_grays[key % RAND_GRAYS];
        TextureRect rect;
        Image img = create_img(w, h);
        image_draw_rect(img, (Rectangle) {0, 0, img.width, img.height}, (Color) {gray, gray, gray, 255});
        TaError error = texture_atlas_add_get_rect(texture_atlas, key, img, 0, 0, &rect);
        if (error){
            printf("error: %i\n", error);
            fflush(stdout);
        } 
        assert(!error);
        free(img.data);
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

static void png_write_hack(TextureAtlas TA, const char *path) {
    SDL_Surface *ta_surface = SDL_CreateSurfaceFrom(TA.image.width, TA.image.height, SDL_PIXELFORMAT_INDEX8, TA.image.data, TA.image.width);
    if (ta_surface == NULL) {
        SDL_Log("Couldn't create surface: %s", SDL_GetError());
        exit(1);
    }
    SDL_Palette *grayscale_palette = SDL_CreateSurfacePalette(ta_surface);
    SDL_SetPaletteColors(grayscale_palette, grayscale_ramp, 0, 256);
    SDL_SavePNG(ta_surface, path);
    SDL_DestroySurface(ta_surface);
}

void test_golden_skyline() {
    TextureAtlas TA = texture_atlas_create(128);
    add_dummy_rect(&TA, 800);
    const char *golden_path = TEST_DATA_DIR "/golden_skyline.png";
    const char *fail_path = TEST_DATA_DIR "/golden_skyline_FAILED.png";
    if (SDL_GetPathInfo(golden_path, NULL)) {
        SDL_Surface *golden_surface = SDL_LoadPNG(golden_path);
        if (golden_surface == NULL) {
            SDL_Log("could not load png at path %s: %s", golden_path, SDL_GetError());
            exit(1);
        }
        assert(golden_surface->format == SDL_PIXELFORMAT_INDEX8);
        assert(SDL_BYTESPERPIXEL(golden_surface->format) == 1);
        size_t size = golden_surface->w * golden_surface->h;
        assert(size == get_pixel_data_size(TA.image.format) * TA.image.height * TA.image.width);
        uint8_t *ref_row = TA.image.data;
        uint8_t *golden_row = (uint8_t *) golden_surface->pixels;
        for (size_t y = 0; y < TA.image.height; y++, ref_row += TA.image.width, golden_row += golden_surface->pitch) {
            uint8_t *ref_p = ref_row, *golden_p = golden_row;
            for (size_t x = 0; x < TA.image.width; x++, ref_p++, golden_p++) {
                if (*ref_p != *golden_p) {
                    png_write_hack(TA, fail_path);
                    fprintf(stderr, "FAILED IMAGE WRITTEN TO: %s\n", fail_path);
                    exit(1);
                }
            }
        }
        SDL_DestroySurface(golden_surface);
    } else {
        png_write_hack(TA, fail_path);
        printf("NO REFERENCE IMAGE FOUND AT: %s. FAILED IMAGE WRITTEN TO: %s\n", golden_path, fail_path);
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
    free(img.data);
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
    free(img.data);
    
    img = create_img(4, 2);
    error = texture_atlas_add_get_rect(&TA, 1, img, 0, 0, &rect);
    assert(!error);
    assert(rect.x == 0 && rect.y == 2);
    free(img.data);

    img = create_img(6, 2);
    error = texture_atlas_add_get_rect(&TA, 2, img, 0, 0, &rect);
    assert(!error);
    assert(rect.x == 0 && rect.y == 4);
    free(img.data);

    img = create_img(2, 6);
    error = texture_atlas_add_get_rect(&TA, 3, img, 0, 0, &rect);
    assert(!error);
    assert(rect.x == 6 && rect.y == 2);
    free(img.data);

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
    free(img.data);

    img = create_img(4, 4);
    error = texture_atlas_add_get_rect(&TA, 0, img, 0, 0, &rect);
    assert(!error);
    assert(rect.w == 2 && rect.h == 2);
    free(img.data);
    
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
