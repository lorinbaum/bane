#include "bane.h"

#define LEN(x) (sizeof(x) / sizeof *(x))

static char *image_print(Image img) {
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

static void print_unequal(char *test_name, size_t test_id, Image dest, uint8_t *expected_data, size_t idx) {
    char *expected = image_print((Image) {.data = expected_data, .format = dest.format, .width = dest.width, .height = dest.height});
    char *got = image_print(dest);
    size_t row_size = dest.width * get_pixel_data_size(dest.format);
    size_t x = idx % row_size, y = idx / row_size;
    printf("Image draw bounds test %s (id=%lu) failed at x=%lu, y=%lu:\nExpected:\n%s\nGot:\n%s\n", test_name, test_id, x, y, expected, got);
    free(expected);
    free(got);
}

void image_copy(Image *dest, Image src) {
    memcpy(dest, &src, sizeof(Image));
    size_t size = src.width * src.height * get_pixel_data_size(src.format);
    dest->data = malloc(size);
    ensure(dest->data != NULL);
    memcpy(dest->data, src.data, size);
}

struct bounds_case {
    Rectangle dest_rect, src_rect;
    uint8_t *exp_data;
};

static const struct bounds_case bounds_equal_format[] = {
    { // 0: rects cover image and beyond in all dimensions
        .dest_rect = {-1, -1, 5, 5},
        .src_rect = {-1, -1, 5, 5},
        .exp_data = (uint8_t[]) {
            48, 49, 50,   51, 52, 53,   54, 55, 56,    9, 10, 11,
            57, 58, 59,   60, 61, 62,   63, 64, 65,   21, 22, 23,
            66, 67, 68,   69, 70, 71,   72, 73, 74,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 1: rects completeley outside image
        .dest_rect = {-1, -1, 1, 4},
        .src_rect = {-1, -1, 1, 4},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 2: only dest rect completeley outside image
        .dest_rect = {-1, -1, 1, 4},
        .src_rect = {0, 0, 1, 4},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 3: only src rect completeley outside image
        .dest_rect = {0, 0, 1, 4},
        .src_rect = {-1, -1, 1, 4},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 4: cropped rects invisible
        .dest_rect = {-1, -1, 2, 3},
        .src_rect = {2, 2, 2, 3},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 5: cropped rects visible top left
        .dest_rect = {-1, -1, 2, 3},
        .src_rect = {1, 1, 2, 3},
        .exp_data = (uint8_t[]) {
            72, 73, 74,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 6: cropped rects visible top right
        .dest_rect = {2, -1, 2, 3},
        .src_rect = {1, 1, 2, 3},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,   69, 70, 71,   72, 73, 74,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 7: cropped rects visible bottom left
        .dest_rect = {-1, 2, 2, 3},
        .src_rect = {1, 1, 2, 3},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            63, 64, 65,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            72, 73, 74,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 8: cropped rects visible bottom right
        .dest_rect = {2, 2, 2, 3},
        .src_rect = {1, 1, 2, 3},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   60, 61, 62,   63, 64, 65,
            36, 37, 38,   39, 40, 41,   69, 70, 71,   72, 73, 74
        }
    },
    { // 9: normal
        .dest_rect = {2, 2, 1, 1},
        .src_rect = {1, 1, 1, 1},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   60, 61, 62,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
};

static const struct bounds_case bounds_alpha_rgb[] = {
    { // 0: rects cover image and beyond in all dimensions
        .dest_rect = {-1, -1, 5, 5},
        .src_rect = {-1, -1, 5, 5},
        .exp_data = (uint8_t[]) {
            255, 255, 255,   255, 255, 255,    255, 255, 255,    9, 10, 11,
            255, 255, 255,   255, 255, 255,    255, 255, 255,   21, 22, 23,
            255, 255, 255,   255, 255, 255,    255, 255, 255,   33, 34, 35,
             36,  37,  38,    39,  40,  41,     42,  43,  44,   45, 46, 47
        }
    },
    { // 1: rects completeley outside image
        .dest_rect = {-1, -1, 1, 4},
        .src_rect = {-1, -1, 1, 4},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 2: only dest rect completeley outside image
        .dest_rect = {-1, -1, 1, 4},
        .src_rect = {0, 0, 1, 4},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 3: only src rect completeley outside image
        .dest_rect = {0, 0, 1, 4},
        .src_rect = {-1, -1, 1, 4},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 4: cropped rects invisible
        .dest_rect = {-1, -1, 2, 3},
        .src_rect = {2, 2, 2, 3},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 5: cropped rects visible top left
        .dest_rect = {-1, -1, 2, 3},
        .src_rect = {1, 1, 2, 3},
        .exp_data = (uint8_t[]) {
            255, 255, 255,    3,  4,  5,    6,  7,  8,    9, 10, 11,
             12,  13,  14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
             24,  25,  26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
             36,  37,  38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 6: cropped rects visible top right
        .dest_rect = {2, -1, 2, 3},
        .src_rect = {1, 1, 2, 3},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,   255, 255, 255,   255, 255, 255,
            12, 13, 14,   15, 16, 17,    18,  19,  20,    21,  22,  23,
            24, 25, 26,   27, 28, 29,    30,  31,  32,    33,  34,  35,
            36, 37, 38,   39, 40, 41,    42,  43,  44,    45,  46,  47
        }
    },
    { // 7: cropped rects visible bottom left
        .dest_rect = {-1, 2, 2, 3},
        .src_rect = {1, 1, 2, 3},
        .exp_data = (uint8_t[]) {
              0,   1,   2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
             12,  13,  14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            255, 255, 255,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            255, 255, 255,   39, 40, 41,   42, 43, 44,   45, 46, 47
        }
    },
    { // 8: cropped rects visible bottom right
        .dest_rect = {2, 2, 2, 3},
        .src_rect = {1, 1, 2, 3},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,     6,   7,   8,     9,  10,  11,
            12, 13, 14,   15, 16, 17,    18,  19,  20,    21,  22,  23,
            24, 25, 26,   27, 28, 29,   255, 255, 255,   255, 255, 255,
            36, 37, 38,   39, 40, 41,   255, 255, 255,   255, 255, 255
        }
    },
    { // 9: normal
        .dest_rect = {2, 2, 1, 1},
        .src_rect = {1, 1, 1, 1},
        .exp_data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,      6,   7,   8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,     18,  19,  20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,    255, 255, 255,   33, 34, 35,
            36, 37, 38,   39, 40, 41,     42,  43,  44,   45, 46, 47
        }
    },
};

int image_draw_bounds_test() {
    int failed = 0;
    Color tint = {255, 255, 255, 255};
    Image dest_base = {
        .data = (uint8_t[]) {
             0,  1,  2,    3,  4,  5,    6,  7,  8,    9, 10, 11,
            12, 13, 14,   15, 16, 17,   18, 19, 20,   21, 22, 23,
            24, 25, 26,   27, 28, 29,   30, 31, 32,   33, 34, 35,
            36, 37, 38,   39, 40, 41,   42, 43, 44,   45, 46, 47
        },
        .format = IMAGE_FORMAT_RGB,
        .width = 4, .height = 4
    };
    Image src = {
        .data = (uint8_t[]) {
            48, 49, 50,   51, 52, 53,   54, 55, 56,
            57, 58, 59,   60, 61, 62,   63, 64, 65,
            66, 67, 68,   69, 70, 71,   72, 73, 74
        },
        .format = IMAGE_FORMAT_RGB,
        .width = 3, .height = 3
    };

    // RGB TO RGB
    for (size_t i = 0; i < LEN(bounds_equal_format); i++) {
        Image dest; image_copy(&dest, dest_base);
        struct bounds_case c = bounds_equal_format[i];
        image_draw(dest, src, c.dest_rect, c.src_rect, tint);
        for (size_t j = 0; j < dest.width * dest.height * get_pixel_data_size(dest.format); j++) {
            if (dest.data[j] != c.exp_data[j]) {
                print_unequal("Bounds test RGB to RGB", i, dest, c.exp_data, j);
                failed++;
                break;
            }
        }
        free(dest.data);
    }

    // ALPHA TO RGB
    src.format = IMAGE_FORMAT_ALPHA;
    memset(src.data, 255, src.width * src.height * 3); // avoid blending from here on
    for (size_t i = 0; i < LEN(bounds_alpha_rgb); i++) {
        Image dest; image_copy(&dest, dest_base);
        struct bounds_case c = bounds_alpha_rgb[i];
        image_draw(dest, src, c.dest_rect, c.src_rect, tint);
        for (size_t j = 0; j < dest.width * dest.height * get_pixel_data_size(dest.format); j++) {
            if (dest.data[j] != c.exp_data[j]) {
                print_unequal("Bounds test ALPHA to RGB", i, dest, c.exp_data, j);
                failed++;
                break;
            }
        }
        free(dest.data);
    }
    return failed;

    // ALPHA TO ALPHA
    dest_base.format = IMAGE_FORMAT_ALPHA; dest_base.width *= 3;
    for (size_t i = 0; i < LEN(bounds_equal_format); i++) {
        Image dest; image_copy(&dest, dest_base);
        struct bounds_case c = bounds_equal_format[i];
        c.dest_rect.x *= 3; c.dest_rect.width *= 3; c.src_rect.x *= 3; c.src_rect.width *= 3;
        image_draw(dest, src, c.dest_rect, c.src_rect, tint);
        for (size_t j = 0; j < dest.width * dest.height * get_pixel_data_size(dest.format); j++) {
            if (dest.data[j] != c.exp_data[j]) {
                print_unequal("Bounds test ALPHA to ALPHA", i, dest, c.exp_data, j);
                failed++;
                break;
            }
        }
        free(dest.data);
    }
}

struct blend_case {
    Color tint;
    uint8_t *dest, *src, *exp;
};

/*  NOTE: The expected values come from tests in GIMP using legacy blend on all layers:
    - GROUP (Opacity: tint.alpha)
        - tint (Blend: multiply)
        - src
    - dest
*/
static const struct blend_case blend_rgb_rgb[] = {
    { // 0: white into black at 40%
        .src = (uint8_t[]) {255, 255, 255},
        .dest = (uint8_t[]) {0, 0, 0},
        .tint = {255, 255, 255, 102},
        .exp = (uint8_t[]) {102, 102, 102}
    },
    { // 1: black into white at 40%
        .src = (uint8_t[]) {0, 0, 0},
        .dest = (uint8_t[]) {255, 255, 255},
        .tint = {255, 255, 255, 102},
        .exp = (uint8_t[]) {153, 153, 153}
    },
    { // 2: white into black at 0%
        .src = (uint8_t[]) {255, 255, 255},
        .dest = (uint8_t[]) {0, 0, 0},
        .tint = {255, 255, 255, 0},
        .exp = (uint8_t[]) {0, 0, 0}
    },
    { // 3: white into gray at 0%
        .src = (uint8_t[]) {255, 255, 255},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {255, 255, 255, 0},
        .exp = (uint8_t[]) {128, 128, 128}
    },
    { // 4: white into gray at 50%
        .src = (uint8_t[]) {255, 255, 255},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {255, 255, 255, 128},
        .exp = (uint8_t[]) {192, 192, 192}
    },
    { // 5: color into gray at 100% with tint
        .src = (uint8_t[]) {106, 39, 208},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {191, 128, 63, 255},
        .exp = (uint8_t[]) {79, 20, 51}
    },
    { // 6: white into gray at 50% with tint
        .src = (uint8_t[]) {255, 255, 255},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {191, 128, 63, 128},
        .exp = (uint8_t[]) {160, 128, 95}
    },
    { // 7: color into gray at 50% with tint
        .src = (uint8_t[]) {106, 39, 208},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {191, 128, 63, 128},
        .exp = (uint8_t[]) {104, 74, 90}
    },
    { // 8: color into color at 70% with tint
        .src = (uint8_t[]) {106, 39, 208},
        .dest = (uint8_t[]) {43, 125, 163},
        .tint = {191, 128, 63, 178},
        .exp = (uint8_t[]) {68, 51, 85} // GIMP gives 52. Expected value before rounding 51.41
    },
};

/*  NOTE: The expected values come from tests in GIMP using legacy blend on all layers:
    - GROUP (Opacity: src)
        - tint (Opacity: tint.alpha)
    - dest
*/
static const struct blend_case blend_alpha_rgb[] = {
    { // 0: white into black at 40%
        .src = (uint8_t[]) {102},
        .dest = (uint8_t[]) {0, 0, 0},
        .tint = {255, 255, 255, 255},
        .exp = (uint8_t[]) {102, 102, 102}
    },
    { // 1: black into white at 40%
        .src = (uint8_t[]) {102},
        .dest = (uint8_t[]) {255, 255, 255},
        .tint = {0, 0, 0, 255},
        .exp = (uint8_t[]) {153, 153, 153}
    },
    { // 2: gray into white at 40%
        .src = (uint8_t[]) {102},
        .dest = (uint8_t[]) {255, 255, 255},
        .tint = {128, 128, 128, 255},
        .exp = (uint8_t[]) {204, 204, 204}
    },
    { // 3: white into gray at 0%
        .src = (uint8_t[]) {0},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {255, 255, 255, 255},
        .exp = (uint8_t[]) {128, 128, 128}
    },
    { // 4: white into gray at 50%
        .src = (uint8_t[]) {128},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {255, 255, 255, 255},
        .exp = (uint8_t[]) {192, 192, 192}
    },
    { // 5: tint into gray at 100%
        .src = (uint8_t[]) {255},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {191, 128, 63, 255},
        .exp = (uint8_t[]) {191, 128, 63}
    },
    { // 6: tint into gray at 50%
        .src = (uint8_t[]) {128},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {191, 128, 63, 255},
        .exp = (uint8_t[]) {160, 128, 95}
    },
    { // 7: 50% tint into gray at 70%
        .src = (uint8_t[]) {178},
        .dest = (uint8_t[]) {128, 128, 128},
        .tint = {191, 128, 63, 128},
        .exp = (uint8_t[]) {150, 128, 105}
    },
    { // 8: 30% tint into color at 70%
        .src = (uint8_t[]) {178},
        .dest = (uint8_t[]) {43, 125, 163},
        .tint = {191, 128, 63, 77},
        .exp = (uint8_t[]) {74, 126, 142}
    },
};

static const struct blend_case blend_alpha_alpha[] = {
    { // 0: white into black at 40%
        .src = (uint8_t[]) {102},
        .dest = (uint8_t[]) {0},
        .tint = {255, 255, 255, 255},
        .exp = (uint8_t[]) {102}
    },
    { // 1: black into white at 40%
        .src = (uint8_t[]) {102},
        .dest = (uint8_t[]) {255},
        .tint = {0, 0, 0, 255},
        .exp = (uint8_t[]) {153}
    },
    { // 2: gray into white at 40%
        .src = (uint8_t[]) {102},
        .dest = (uint8_t[]) {255},
        .tint = {128, 128, 128, 255},
        .exp = (uint8_t[]) {204}
    },
    { // 3: average gray into white at 40%
        .src = (uint8_t[]) {102},
        .dest = (uint8_t[]) {255},
        .tint = {29, 141, 214, 255}, // average 128, 128, 128, 255
        .exp = (uint8_t[]) {204}
    },
    { // 4: white into gray at 0%
        .src = (uint8_t[]) {0},
        .dest = (uint8_t[]) {128},
        .tint = {255, 255, 255, 255},
        .exp = (uint8_t[]) {128}
    },
    { // 5: gray into gray at 100%
        .src = (uint8_t[]) {255},
        .dest = (uint8_t[]) {128},
        .tint = {192, 192, 192, 255},
        .exp = (uint8_t[]) {192}
    },
    { // 6: white into gray at 50%
        .src = (uint8_t[]) {128},
        .dest = (uint8_t[]) {128},
        .tint = {255, 255, 255, 255},
        .exp = (uint8_t[]) {192}
    },
    { // 7: gray into gray at 30%
        .src = (uint8_t[]) {77},
        .dest = (uint8_t[]) {64},
        .tint = {128, 128, 128, 255},
        .exp = (uint8_t[]) {83}
    },
    { // 8: 30% white into gray at 30%
        .src = (uint8_t[]) {77},
        .dest = (uint8_t[]) {128},
        .tint = {255, 255, 255, 77},
        .exp = (uint8_t[]) {140}
    }
};

int image_draw_blend_test() {
    int failed = 0;
    // RGB RGB
    Image dest = {.data = NULL, .format = IMAGE_FORMAT_RGB, .width = 1, .height = 1};
    Image src = {.data = NULL, .format = IMAGE_FORMAT_RGB, .width = 1, .height = 1};
    uint8_t px_bytes = get_pixel_data_size(IMAGE_FORMAT_RGB);
    for (size_t i = 0; i < LEN(blend_rgb_rgb); i++) {
        struct blend_case c = blend_rgb_rgb[i];
        dest.data = c.dest; src.data = c.src;
        image_draw(dest, src, (Rectangle){0,0,1,1}, (Rectangle){0,0,1,1}, c.tint);
        for (size_t p = 0; p < px_bytes; p++) {
            if (dest.data[p] != c.exp[p]) {
                print_unequal("Blend test RGB to RGB", i, dest, c.exp, p);
                failed++;
                break;
            }
        }
    }
    // ALPHA RGB
    src.format = IMAGE_FORMAT_ALPHA;
    for (size_t i = 0; i < LEN(blend_alpha_rgb); i++) {
        struct blend_case c = blend_alpha_rgb[i];
        dest.data = c.dest; src.data = c.src;
        image_draw(dest, src, (Rectangle){0,0,1,1}, (Rectangle){0,0,1,1}, c.tint);
        for (size_t p = 0; p < px_bytes; p++) {
            if (dest.data[p] != c.exp[p]) {
                print_unequal("Blend test ALPHA to RGB", i, dest, c.exp, p);
                failed++;
                break;
            }
        }
    }

    // ALPHA RGB
    dest.format = IMAGE_FORMAT_ALPHA;
    px_bytes = get_pixel_data_size(IMAGE_FORMAT_ALPHA);
    for (size_t i = 0; i < LEN(blend_alpha_alpha); i++) {
        struct blend_case c = blend_alpha_alpha[i];
        dest.data = c.dest; src.data = c.src;
        image_draw(dest, src, (Rectangle){0,0,1,1}, (Rectangle){0,0,1,1}, c.tint);
        for (size_t p = 0; p < px_bytes; p++) {
            if (dest.data[p] != c.exp[p]) {
                print_unequal("Blend test ALPHA to ALPHA", i, dest, c.exp, p);
                failed++;
                break;
            }
        }
    }
    
    return failed;
}

int main(void) {
    int failed = 0;
    failed += image_draw_bounds_test();
    failed += image_draw_blend_test();
    printf("IMAGE TESTS FAILED: %i\n", failed);
    return failed > 0;
}