#include <stdio.h>
#include <math.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "bane.h"

#define SCALE 1.25 // HACK: known scaling on testing machine

int main(void) {
    
    // FREETYPE
    FT_Error error;
    FT_Library  library;
    error = FT_Init_FreeType( &library );
    if ( error ) { printf("... an error occurred during library initialization ..."); }
    
    FT_Face face;
    error = FT_New_Face(library, "assets/FiraSans-Regular.ttf", 0, &face);
    if (error == FT_Err_Unknown_File_Format) { printf("Unknown file format."); }
    else if (error) { printf("Other error occured"); }
    
    error = FT_Set_Char_Size(face, 0, 20*64*SCALE, 72, 72 );
    if (error) { printf("FreeType error setting char size"); }

    FT_GlyphSlot slot = face->glyph;
    FT_UInt glyph_index;
    FT_Int32 load_flags = 0;
    FT_Render_Mode render_mode = FT_RENDER_MODE_NORMAL;
    
    TextureAtlas* glyph_atlas = texture_atlas_create(4096);
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "Bane of my existence");
    
    FT_ULong char_code = FT_Get_First_Char(face, &glyph_index);
    while (glyph_index != 0) {
        if (!FT_Load_Glyph(face, glyph_index, load_flags)) {
            if (!FT_Render_Glyph(slot, render_mode)) {
                if (slot->bitmap.width != 0 && slot->bitmap.rows != 0) {
                    TextureRect rect;
                    assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding not supported rn
                    Image texture = (Image) { (void*) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE };
                    // NOTE: UnloadImage(texture) is not called because all it would do is free the data pointer, which isn't desired.
                    TAStatus status = texture_atlas_add_get_rect(&rect, glyph_atlas, char_code, texture, slot->bitmap_left, slot->bitmap_top);
                    if (status == TA_MAX_SIZE_EXCEEDED) {
                        printf("Texture atlas max size exceeded %li\n", char_code);
                        exit(1);
                    }
                }
            }
            
        }
        char_code = FT_Get_Next_Char(face, char_code, &glyph_index);
    }

    // ExportImage(glyph_atlas->image, "glyph_atlas.jpg");

    Color text_color = { 0xe3, 0x88, 0x64, 0xff };

    while (!WindowShouldClose())
    {
        float pen_x = 10, pen_y = 40;
        BeginDrawing();
            ClearBackground(BLACK);
            char_code = FT_Get_First_Char(face, &glyph_index);
            while (glyph_index != 0) {
                error = FT_Load_Glyph(face, glyph_index, load_flags );
                if (error) { continue; }
                if (pen_x + (slot->advance.x >> 6) > GetScreenWidth()) {
                    pen_x = 0;
                    pen_y += 30;
                }
                TAStatus status = texture_atlas_draw(glyph_atlas, char_code, pen_x, pen_y, text_color);
                if (status != TA_OK) {
                    char_code = FT_Get_Next_Char(face, char_code, &glyph_index);
                    continue;
                }
                pen_x += slot->advance.x >> 6;
                char_code = FT_Get_Next_Char(face, char_code, &glyph_index);
            }
        EndDrawing();
    }

    texture_atlas_destroy(&glyph_atlas);
    CloseWindow();
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    return 0;
}