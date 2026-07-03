#include <stdio.h>
#include <math.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "bane.h"
#include "firasans_regular.c"

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
    
    TextureAtlas *glyph_atlas = texture_atlas_create(4096);
    
    // char *text = u8"assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported\n"
    // "Image texture = {(void *) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};\n"
    // "status = texture_atlas_add_get_rect(&rect, atlas, glyph_index, texture, slot->bitmap_left, slot->bitmap_top);\n"
    // "assert(status == TA_OK);  ";
    char *text = u8"\na\naaaaa\n\na\n";
    size_t len, processed_bytes;
    
    if (utf8_measure_codepoints(text, strlen(text), &len, &processed_bytes) != UTF8_OK) { return 1; }
    uint32_t codepoints[len];
    if (utf8_decode(text, strlen(text), true, len, codepoints, &len) != UTF8_OK) { return 1; }
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "Bane of my existence");

    Cursor cursor = { 0 };
    TextBox textbox = { .x = 20, .y = 0, .w = 50, .h = 600, .codepoints = codepoints, .codepoint_count = len };

    Style style = {
        .atlas = glyph_atlas,
        .cursor_color = WHITE,
        .face = face,
        .line_height = 1,
        .selection_color = (Color) { 255, 0, 0, 128 },
        .text_color = (Color) { 0xe3, 0x88, 0x64, 0xff }
    };

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        draw_text(textbox, style);
        bool selecting = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (IsKeyPressed(KEY_RIGHT)) cursor_right(textbox, &cursor, selecting);
        if (IsKeyPressed(KEY_LEFT)) cursor_left(&cursor, selecting);
        if (IsKeyPressed(KEY_DOWN)) cursor_down(textbox, style, &cursor, selecting); 
        if (IsKeyPressed(KEY_UP)) cursor_up(textbox, style, &cursor, selecting);
        
        draw_cursor(textbox, style, &cursor);
        DrawLineEx((Vector2) {textbox.x + textbox.w, textbox.y}, (Vector2) {textbox.x + textbox.w, textbox.y + textbox.h}, 2, WHITE);
        EndDrawing();
    }

    // ExportImage(glyph_atlas->image, "glyph_atlas.jpg");
    
    texture_atlas_destroy(&glyph_atlas);
    CloseWindow();
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    return 0;
}