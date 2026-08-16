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
    rect_fit_box((Rectangle) {0, 0, min(src.width, dest.width), min(src.height, dest.height)}, &rect, NULL);
    assert(dest.format == src.format);
    uint8_t px_bytes = get_pixel_data_size(src.format);
    uint8_t row_bytes_dest = dest.width * px_bytes;
    uint8_t row_bytes_src = src.width * px_bytes;
    uint8_t row_bytes_rect = rect.width * px_bytes;
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
                for (uint c = 0; c < px_bytes_dest; c++) dest_p[c] = blend_value(dest_p[c], src_p[c] * tint_norm[c], alpha) + .5;
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
                for (uint c = 0; c < px_bytes_dest; c++) dest_p[c] = blend_value(dest_p[c], tint_c[c], blend_alpha) + .5;
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
    image_draw(resized, image, x, y);
    return resized;
}
