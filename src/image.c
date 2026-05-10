#include "spiritlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "helpers.h"

#define BMP_HEADER_SIZE 54
#define DIB_HEADER_SIZE 40

static void write_uint16_le(uint8_t* buf, uint32_t index, uint16_t val) {
    buf[index] = val & 0xFF;
    buf[index+1] = (val >> 8) & 0xFF;
}
static void write_uint32_le(uint8_t* buf, uint32_t index, uint32_t val) {
    buf[index] = val & 0xFF;
    buf[index+1] = (val >> 8) & 0xFF;
    buf[index+2] = (val >> 16) & 0xFF;
    buf[index+3] = (val >> 24) & 0xFF;
}

SS_Status bmp_write(const char* path, const uint8_t* data, uint32_t w, uint32_t h, uint32_t c) {
    if (path == NULL || data == NULL) { return report(BMP_ARGUMENT_ERROR, "Path = NULL or data == NULL\n"); }
    if (w > INT32_MAX || h > INT32_MAX) { return report(BMP_ARGUMENT_ERROR, "width = %u, height = %u exceeds INT32_MAX\n", w, h); } 
    if (c != 1 && c != 3) { return report(BMP_ARGUMENT_ERROR, "1 or 3 channels supported, got %u\n", c); } // if 1 channel, will expand to three channels.
    SS_Status status = report(BMP_OK, "\n");
    uint8_t* buffer;
    size_t written;
    size_t buffer_idx;
    FILE* fp = fopen(path, "wb");
    if (fp == NULL) { return report(BMP_FOPEN_ERROR, "Failed to open: %s\n", path); }
    uint32_t row_padding = (4 - (w * 3) % 4) % 4;
    size_t bmp_size = ((size_t) w * 3 + row_padding) * h + BMP_HEADER_SIZE;
    if (bmp_size > UINT32_MAX) {
        status = report(BMP_SIZE_ERROR, "Size %lu exceeds max UINT32_MAX\n", bmp_size);
        goto cleanup_file;
    }
    buffer = (uint8_t*) calloc(1, bmp_size);
    assert(buffer != NULL);

    // FILE HEADER
    buffer[0] = 'B';
    buffer[1] = 'M';
    write_uint32_le(buffer, 2, bmp_size);
    // bytes 6, 7, 8, 9 are 0
    write_uint32_le(buffer, 10, BMP_HEADER_SIZE);

    // DIB HEADER
    write_uint32_le(buffer, 14, DIB_HEADER_SIZE);
    write_uint32_le(buffer, 18, w);
    write_uint32_le(buffer, 22, h);
    write_uint16_le(buffer, 26, 1); // color panes, must be 1
    write_uint16_le(buffer, 28, 24); // bits per pixel
    write_uint32_le(buffer, 30, 0); // compression method (0 = none)
    write_uint32_le(buffer, 34, 0); // image size. can be 0 if no compression
    write_uint32_le(buffer, 38, 0); // horizontal resolution (pixel per meter)
    write_uint32_le(buffer, 42, 0); // vertical resolution (pixel per meter)
    write_uint32_le(buffer, 46, 0); // number of colors in color palette. 0 to use default
    write_uint32_le(buffer, 50, 0); // import colors. every color is important

    buffer_idx = BMP_HEADER_SIZE;
    for (int row = h - 1; row >= 0; row--) {
        for (int col = 0; col < w; col++) {
            uint32_t data_idx = (row*w+col)*c;
            if (c == 1) {
                buffer[buffer_idx++] = data[data_idx];
                buffer[buffer_idx++] = data[data_idx];
                buffer[buffer_idx++] = data[data_idx];
            }
            else { // c = 3
                buffer[buffer_idx++] = data[data_idx+2]; // B
                buffer[buffer_idx++] = data[data_idx+1]; // G
                buffer[buffer_idx++] = data[data_idx];   // R
            }
        }
        for (int pad = 0; pad < row_padding; pad++) { buffer[buffer_idx++] = 0; }
    }

    written = fwrite(buffer, sizeof(uint8_t), bmp_size, fp);
    if (written != bmp_size) { status = report(BMP_FWRITE_ERROR, "Failed to write to %s. Wrote %i / %i\n", path, written, bmp_size); }

    free(buffer);
cleanup_file:
    fclose(fp);
    return status;
}