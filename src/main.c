#include <stdio.h>
#include <math.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "bane.h"
#include "firasans_regular.c"

#define SCALE 1.25 // HACK: known scaling on testing machine

int main(void) {
    
    // FREETYPE
    FT_Error error;
    FT_Library  library;
    error = FT_Init_FreeType( &library );
    assert(!error);
    
    FT_Face face;
    error = FT_New_Memory_Face(library, (FT_Byte *) assets_FiraSans_Regular_ttf, assets_FiraSans_Regular_ttf_len, 0, &face);
    assert(!error);
    
    error = FT_Set_Char_Size(face, 0, 20*64*SCALE, 72, 72 );
    assert(!error);
    FT_GlyphSlot slot = face->glyph; // shorthand
    
    TextureAtlas *glyph_atlas = texture_atlas_create(4096);
    
    // ExportImage(glyph_atlas->image, "glyph_atlas.jpg");

    char *text = u8"Hamödi←↑→↓↓↓↓";
    uint32_t codepoints[50];
    size_t len;
    UTF8Status utf8status = utf8_decode(text, strlen(text), true, 50, codepoints, &len);
    printf("    LEN: %lu\n", len);
    assert(utf8status == UTF8_OK);
    char encoded[100];
    size_t len2;
    utf8status = utf8_encode(codepoints, len, true, 100, encoded, &len2);
    printf("    STATUS %i, LEN %lu, strlen: %lu, ENCODED: %s\n", utf8status, len2, strlen(text), encoded);
    assert(utf8status == UTF8_OK);

    Color text_color = { 0xe3, 0x88, 0x64, 0xff };

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "Bane of my existence");

    while (!WindowShouldClose())
    {
        float pen_x = 10, pen_y = 40;
        BeginDrawing();
            ClearBackground(BLACK);
            for (size_t i = 0; i < len; i++) {
                FT_UInt glyph_index = FT_Get_Char_Index(face, codepoints[i]);
                TextureRect rect;
                TAStatus status = texture_atlas_get_rect(&rect, glyph_atlas, glyph_index);
                if (status == TA_RECT_NOT_FOUND) {
                    error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
                    assert(!error);
                    assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported
                    Image texture = (Image) {(void *) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
                    status = texture_atlas_add_get_rect(&rect, glyph_atlas, glyph_index, texture, slot->bitmap_left, slot->bitmap_top);
                    assert(status == TA_OK);                    
                }
                error = FT_Load_Glyph(face, glyph_index, FT_LOAD_BITMAP_METRICS_ONLY);
                assert(!error);
                if (pen_x + ((slot->advance.x + 32) >> 6) > GetScreenWidth()) {
                    pen_x = 0;
                    pen_y += face->size->metrics.height >> 6;
                }
                status = texture_atlas_draw(glyph_atlas, glyph_index, pen_x, pen_y, text_color);
                assert(status == TA_OK);
                pen_x += (slot->advance.x + 32) >> 6;
            }
        EndDrawing();
    }

    texture_atlas_destroy(&glyph_atlas);
    CloseWindow();
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    return 0;
}