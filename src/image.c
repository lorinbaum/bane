#include "bane.h"

uint8_t get_pixel_data_size(ImageFormat format) {
    switch (format) {
        case IMAGE_FORMAT_ALPHA: return 1;
        case IMAGE_FORMAT_RGB: return 3;
        default: abort();
    }
}

// blend c1 into c0 at opacity of alpha (0...1)
static float blend_value(float c0, float c1, float alpha) {
    assert(alpha >= 0 && alpha <= 1);
    return c0 * (1 - alpha) + c1 * alpha;
}

Image image_create(size_t width, size_t height, ImageFormat format) {
    Image ret = {.width = width, .height = height, .format = format, .data = malloc(get_pixel_data_size(format) * width * height)};
    ensure(ret.data != NULL);
    return ret;
}
Image image_create_from_data(uint8_t *data, size_t width, size_t height, ImageFormat format) {
    return (Image) {.data = data, .width = width, .height = height, .format = format};
}
void image_destroy(Image *img) {
    free(img->data);
    memset(img, 0, sizeof(Image));
}

void image_draw(Image dest, Image src, int_least32_t x, int_least32_t y) {
    Rectangle rect = {x, y, src.width, src.height};
    rect_fit_box((Rectangle) {0, 0, dest.width, dest.height}, &rect, NULL);
    assert(dest.format == src.format);
    uint8_t px_bytes = get_pixel_data_size(src.format);
    size_t row_bytes_dest = dest.width * px_bytes;
    size_t row_bytes_src = src.width * px_bytes;
    size_t row_bytes_rect = rect.width * px_bytes;
    uint8_t *dest_p = dest.data + (rect.y * dest.width + rect.x) * px_bytes;
    uint8_t *src_p = src.data;
    for (int_least32_t row = 0; row < rect.height; row++, dest_p += row_bytes_dest, src_p += row_bytes_src) memmove(dest_p, src_p, row_bytes_rect);
}

void image_copy(Image *dest, Image src) {
    if (dest->data == src.data) return;
    *dest = image_create(src.width, src.height, src.format);
    image_draw(*dest, src, 0, 0);
}

void image_draw_tint(Image dest, Image src, Rectangle dest_rect, Rectangle src_rect, Color tint) {
    assert(!(src.format == IMAGE_FORMAT_RGB && dest.format == IMAGE_FORMAT_ALPHA));
    assert(dest.format == IMAGE_FORMAT_ALPHA || dest.format == IMAGE_FORMAT_RGB);
    assert(src.format == IMAGE_FORMAT_ALPHA || src.format == IMAGE_FORMAT_RGB);
    assert(dest.data != NULL && src.data != NULL);
    assert(color_is_valid(tint));

    bool resample = src_rect.width != dest_rect.width || src_rect.height != dest_rect.height;
    assert(!resample);

    rect_fit_box((Rectangle) {0, 0, dest.width, dest.height}, &dest_rect, &src_rect);
    rect_fit_box((Rectangle) {0, 0, src.width, src.height}, &src_rect, &dest_rect);
    
    if (src_rect.width <= 0 || src_rect.height <= 0) return;
    int_least32_t rect_width = src_rect.width, rect_height = src_rect.height;

    uint8_t px_bytes_src = get_pixel_data_size(src.format);
    uint8_t px_bytes_dest = get_pixel_data_size(dest.format);

    size_t next_row_bytes_src = (src.width - rect_width) * px_bytes_src; // from right edge of src_rect to start of next rect row
    size_t next_row_bytes_dest = (dest.width - rect_width) * px_bytes_dest; // from right edge of dest_rect to start of next rect row

    uint8_t *src_p = src.data + (src_rect.y * src.width + src_rect.x) * px_bytes_src;
    uint8_t *dest_p = dest.data + (dest_rect.y * dest.width + dest_rect.x) * px_bytes_dest;

    if (dest.format == IMAGE_FORMAT_RGB && src.format == IMAGE_FORMAT_RGB) {
        float alpha = tint.a / 255.0f;
        float tint_norm[3] = {tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f};
        for (int_least32_t y = 0; y < rect_height; y++, src_p += next_row_bytes_src, dest_p += next_row_bytes_dest) {
            for (int_least32_t x = 0; x < rect_width; x++, src_p += px_bytes_src, dest_p += px_bytes_dest) {
                for (uint8_t c = 0; c < px_bytes_dest; c++) dest_p[c] = blend_value(dest_p[c], src_p[c] * tint_norm[c], alpha) + .5;
            }
        }
    } else {
        assert(src.format == IMAGE_FORMAT_ALPHA);
        float alpha = tint.a / 255.0f;
        float tint_c[3] = {tint.r, tint.g, tint.b};
        if (dest.format == IMAGE_FORMAT_ALPHA) tint_c[0] = (tint.r + tint.g + tint.b) / 3.0f;
        for (int_least32_t y = 0; y < rect_height; y++, src_p += next_row_bytes_src, dest_p += next_row_bytes_dest) {
            for (int_least32_t x = 0; x < rect_width; x++, src_p += px_bytes_src, dest_p += px_bytes_dest) {
                float blend_alpha = alpha * *src_p / 255.0f;
                for (uint8_t c = 0; c < px_bytes_dest; c++) dest_p[c] = blend_value(dest_p[c], tint_c[c], blend_alpha) + .5;
            }
        }
    }
}

void image_draw_rect(Image dest, Rectangle rect, Color color) {
    assert(color_is_valid(color));
    rect_fit_box((Rectangle) {0, 0, dest.width, dest.height}, &rect, NULL);
    if (rect.width <= 0 || rect.height <= 0) return;

    uint8_t px_bytes = get_pixel_data_size(dest.format);
    size_t bytes_next_row_dest = (dest.width - rect.width) * px_bytes;
    uint8_t *dest_p = dest.data + (rect.y * dest.width + rect.x) * px_bytes;

    float alpha = color.a / 255.0f;
    float color_c[3] = {color.r, color.g, color.b};
    if (dest.format == IMAGE_FORMAT_ALPHA) color_c[0] = (color.r + color.g + color.b) / 3;
    for (int_least32_t y = 0; y < rect.height; y++, dest_p += bytes_next_row_dest) {
        for (int_least32_t x = 0; x < rect.width; x++, dest_p += px_bytes) {
            for (uint8_t c = 0; c < px_bytes; c++) dest_p[c] = blend_value(dest_p[c], color_c[c], alpha) + .5;
        }
    }
}

Image image_resize(Image image, int_least32_t w, int_least32_t h, int_least32_t x, int_least32_t y, Color color) {
    assert(w > 0 && h > 0);
    assert(color_is_valid(color));
    Image resized = image_create(w, h, image.format);
    image_draw_rect(resized, (Rectangle) {0, 0, x, resized.height}, color);
    image_draw_rect(resized, (Rectangle) {x + image.width, 0, resized.width - (x + image.width), resized.height}, color);
    image_draw_rect(resized, (Rectangle) {x, 0, image.width, y}, color);
    image_draw_rect(resized, (Rectangle) {x, y + image.height, image.width, resized.height - (y + image.height)}, color);
    // NOTE: Beware of using image_draw_tint: the part of resized where image will go is uninitialized memory and may blend with image
    image_draw(resized, image, x, y);
    return resized;
}

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

void image_write_png(Image img, const char *path) {
    if (img.format == IMAGE_FORMAT_ALPHA) {
        SDL_Surface *surface = SDL_CreateSurfaceFrom(img.width, img.height, SDL_PIXELFORMAT_INDEX8, img.data, img.width);
        if (surface == NULL) {
            SDL_Log("Couldn't create surface: %s", SDL_GetError());
            exit(1);
        }
        SDL_Palette *grayscale_palette = SDL_CreateSurfacePalette(surface);
        SDL_SetPaletteColors(grayscale_palette, grayscale_ramp, 0, 256);
        SDL_SavePNG(surface, path);
        SDL_DestroySurface(surface);
    } else assert(0); // not implemented
}

char *image_write_string(Image img) {
    uint8_t px_bytes = get_pixel_data_size(img.format);
    size_t len = 
        (
            img.width * px_bytes * 4 // 3 digits + 1 space per pixel
            + (img.width - 1) * 3 // 3 spaces between pixels
            + 1 // \n
        ) 
        * img.height
        + 1; // '\0'
    char *ret = malloc(len * sizeof(char));
    ensure(ret != NULL);
    size_t written = 0;
    for (size_t y = 0; y < img.height; y++) {
        for (size_t x = 0; x < img.width; x++) {
            for (uint8_t c = 0; c < px_bytes; c++, written += 4) {
                sprintf(ret + written, "%3u ", img.data[(y * img.width + x) * px_bytes + c]);
            }
            if (x + 1 < img.width) {
                sprintf(ret + written, "    ");
                written += 3;
            } 
        }
        sprintf(ret + written, "\n");
        written++;
    }
    ret[len - 1] = '\0';
    return ret;
}