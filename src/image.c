#include "bane.h"

uint8_t get_pixel_data_size(ImageFormat format) {
    switch (format) {
        case IMAGE_FORMAT_ALPHA: return 1;
        case IMAGE_FORMAT_RGB: return 3;
        default: return 0;
    }
}

void image_draw(Image dest, Image src, Rectangle dest_rect, Rectangle src_rect, Color tint) {
    assert(!(src.format == IMAGE_FORMAT_RGB && dest.format == IMAGE_FORMAT_ALPHA));
    assert(dest.format == IMAGE_FORMAT_ALPHA || dest.format == IMAGE_FORMAT_RGB);
    assert(src.format == IMAGE_FORMAT_ALPHA || src.format == IMAGE_FORMAT_RGB);
    if (dest.data == NULL || src.data == NULL) return;

    bool resample = src_rect.width != dest_rect.width || src_rect.height != dest_rect.height;
    assert(!resample);

    // NOTE: this only works if no scaling is involved because transformations applied to linked_fit are not scaled either.
    rect_fit_box((Rectangle) {0, 0, dest.width, dest.height}, &dest_rect, &src_rect);
    rect_fit_box((Rectangle) {0, 0, src.width, src.height}, &src_rect, &dest_rect);

    if (src_rect.width <= 0 || src_rect.height <= 0) return;

    uint8_t px_bytes_src = get_pixel_data_size(src.format);
    assert(px_bytes_src != 0);
    uint8_t px_bytes_dest = get_pixel_data_size(dest.format);
    assert(px_bytes_dest != 0);
    size_t row_bytes_src = px_bytes_src * src.width;
    size_t row_bytes_dest = px_bytes_dest * dest.width;

    uint8_t *src_row = src.data + (src_rect.y * src.width + src_rect.x) * px_bytes_src;
    uint8_t *dest_row = dest.data + (dest_rect.y * dest.width + dest_rect.x) * px_bytes_dest;

    if (dest.format == IMAGE_FORMAT_RGB && src.format == IMAGE_FORMAT_RGB) {
        float blend_dest = 1 - tint.a / 255.0f;
        float blend_r = tint.r / 255.0f * tint.a / 255.0f;
        float blend_g = tint.g / 255.0f * tint.a / 255.0f;
        float blend_b = tint.b / 255.0f * tint.a / 255.0f;
        for (int_least32_t y = 0; y < src_rect.height; y++, src_row += row_bytes_src, dest_row += row_bytes_dest) {
            uint8_t *src_p = src_row, *dest_p = dest_row;
            for (int_least32_t x = 0; x < src_rect.width; x++, dest_p += px_bytes_dest, src_p += px_bytes_src) {
                *dest_p       = *dest_p       * blend_dest + *src_p       * blend_r + .5;
                *(dest_p + 1) = *(dest_p + 1) * blend_dest + *(src_p + 1) * blend_g + .5;
                *(dest_p + 2) = *(dest_p + 2) * blend_dest + *(src_p + 2) * blend_b + .5;
            }
        }
    } else if (dest.format == IMAGE_FORMAT_RGB && src.format == IMAGE_FORMAT_ALPHA) {
        float a = tint.a / 255.0f;
        for (int_least32_t y = 0; y < src_rect.height; y++, src_row += row_bytes_src, dest_row += row_bytes_dest) {
            uint8_t *src_p = src_row, *dest_p = dest_row;
            for (int_least32_t x = 0; x < src_rect.width; x++, dest_p += px_bytes_dest, src_p += px_bytes_src) {
                float blend_src = a * *src_p / 255.0f;
                float blend_dest = 1 - blend_src;
                *dest_p       = *dest_p       * blend_dest + blend_src * tint.r + .5;
                *(dest_p + 1) = *(dest_p + 1) * blend_dest + blend_src * tint.g + .5;
                *(dest_p + 2) = *(dest_p + 2) * blend_dest + blend_src * tint.b + .5;
            }
        }
    } else {
        assert(dest.format == IMAGE_FORMAT_ALPHA && src.format == IMAGE_FORMAT_ALPHA);
        float gray = (tint.r + tint.g + tint.b) / 3.0f;
        float a = tint.a / 255.0f;
        for (int_least32_t y = 0; y < src_rect.height; y++, src_row += row_bytes_src, dest_row += row_bytes_dest) {
            uint8_t *src_p = src_row, *dest_p = dest_row;
            for (int_least32_t x = 0; x < src_rect.width; x++, dest_p += px_bytes_dest, src_p += px_bytes_src) {
                float blend_src = a * *src_p / 255.0f;
                float blend_dest = 1 - blend_src;
                *dest_p = *dest_p * blend_dest + blend_src * gray + .5;
            }
        }
    }
}

static void get_p(ImageFormat format, Color color, uint8_t *out) {
    if (format == IMAGE_FORMAT_ALPHA) out[0] = (color.r + color.g + color.b) / 3;
    else {
        assert(format == IMAGE_FORMAT_RGB);
        out[0] = color.r; out[1] = color.g; out[2] = color.b;
    }
}

static void get_blend(ImageFormat format, Color color, float *out) {
    float alpha = color.a / 255.0f;
    if (format == IMAGE_FORMAT_ALPHA) out[0] = (color.r + color.g + color.b) / (3 * 255.0f) * alpha;
    else {
        assert(format == IMAGE_FORMAT_RGB);
        out[0] = color.r / 255.0f * alpha; out[1] = color.g / 255.0f * alpha; out[2] = color.b / 255.0f * alpha;
    }
}

void image_draw_rect(Image dest, Rectangle rect, Color color) {
    // blending a color onto IMAGE_FORMAT_ALPHA uses average of r,g,b channels. This is to keep blended-in color separate from alpha.
    rect_fit_box((Rectangle) {0, 0, dest.width, dest.height}, &rect, NULL);

    if (rect.width <= 0 || rect.height <= 0) return;

    uint8_t px_bytes = get_pixel_data_size(dest.format);
    assert(px_bytes != 0);
    size_t row_bytes = px_bytes * dest.width;
    uint8_t *dest_row = dest.data + (rect.y * row_bytes) + rect.x * px_bytes;

    if (color.a == 255) {
        if ((dest.format == IMAGE_FORMAT_RGB && color.r == color.g && color.g == color.b) || dest.format == IMAGE_FORMAT_ALPHA) {
            uint8_t v = dest.format == IMAGE_FORMAT_ALPHA ? (color.r + color.g + color.b) / 3 : color.r;
            for (int_least32_t y = 0; y < rect.height; y++, dest_row += row_bytes) memset(dest_row, v, rect.width * px_bytes);
        } else {
            uint8_t p[px_bytes];
            get_p(dest.format, color, (uint8_t *) p);
            for (int_least32_t y = 0; y < rect.height; y++, dest_row += row_bytes) {
                uint8_t *dest_p = dest_row;
                for (int_least32_t x = 0; x < rect.width; x++, dest_p += px_bytes) memcpy(dest_p, p, px_bytes);
            }
        }
    } else {
        uint8_t p[px_bytes];
        get_p(dest.format, color, (uint8_t *) p);
        float blend_dest = 1 - color.a / 255.0f;
        float blend_src[px_bytes];
        get_blend(dest.format, color, (float *) blend_src);
        for (int_least32_t y = 0; y < rect.height; y++, dest_row += row_bytes) {
            uint8_t *dest_p = dest_row;
            for (int_least32_t x = 0; x < rect.width; x++, dest_p += px_bytes) {
                for (uint8_t c = 0; c < px_bytes; c++) *(dest_p + c) = *(dest_p + c) * blend_dest + p[c] * blend_src[c];
            }
        }
    }
}

void image_resize(Image *image, int_least32_t new_w, int_least32_t new_h, int_least32_t offset_x, int_least32_t offset_y, Color color) {
    assert(new_w > 0 && new_h > 0);
    uint8_t px_bytes = get_pixel_data_size(image->format);
    assert(px_bytes != 0);
    Image resized = (Image) {.data = malloc(new_w * new_h * px_bytes), .format = image->format, .height = new_h, .width = new_w};
    ensure(resized.data != NULL);
    image_draw_rect(resized, (Rectangle) {0, 0, resized.width, resized.height}, color);
    image_draw(resized, *image,
        (Rectangle) {offset_x, offset_y, image->width, image->height},
        (Rectangle) {0, 0, image->width, image->height}, (Color) {255, 255, 255, 255}
    );
    free(image->data);
    memcpy(image, &resized, sizeof(Image));
}
