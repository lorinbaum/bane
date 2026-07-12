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
    
    char *text = u8"assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported\n"
    "Image texture = {(void *) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};\n"
    "status = texture_atlas_add_get_rect(&rect, atlas, glyph_index, texture, slot->bitmap_left, slot->bitmap_top);\n"
    "assert(status == TA_OK);\n\n"
    "assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported\na";
    // char *text = u8"\na\naaaaa\n\na\n";
    size_t len, processed_bytes;
    
    if (utf8_measure_codepoints(text, strlen(text), &len, &processed_bytes) != UTF8_OK) { return 1; }
    uint32_t codepoints[len];
    if (utf8_decode(text, strlen(text), true, len, codepoints, &len) != UTF8_OK) { return 1; }
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "Bane of my existence");

    Cursor cursor = { 0 };
    TextBox textbox = { .x = 50, .y = 50, .w = GetScreenWidth() - 100, .h = 100, .codepoints = codepoints, .codepoint_count = len };

    Style style = {
        .cursor_color = WHITE,
        .face = face,
        .line_height = 1,
        .selection_color = (Color) { 0xff, 0, 0, 0x60 },
        .text_color = (Color) { 0xe3, 0x88, 0x64, 0xff },
        .text_selected_color = (Color) { 0xe3, 0x88, 0x64, 0xff }
    };

    TextBox *last_hit = NULL;

    while (!WindowShouldClose()) {
        if (IsWindowResized()) textbox.w = GetScreenWidth() - 100;
        BeginDrawing();
        ClearBackground(BLACK);
        draw_text(&textbox, style, glyph_atlas, &cursor);
        bool selecting = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
            if (ctrl) cursor_next_word(textbox, &cursor, selecting);
            else cursor_right(textbox, &cursor, selecting);
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
            if (ctrl) cursor_prev_word(textbox, &cursor, selecting);
            else cursor_left(&cursor, selecting);
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) cursor_down(textbox, style, &cursor, selecting); 
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) cursor_up(textbox, style, &cursor, selecting);
        if (IsKeyPressed(KEY_HOME) || IsKeyPressedRepeat(KEY_HOME)) cursor_home(textbox, style, &cursor, selecting);
        if (IsKeyPressed(KEY_END) || IsKeyPressedRepeat(KEY_END)) cursor_end(textbox, style, &cursor, selecting);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 pos = GetMousePosition();
            int_least32_t x = (int_least32_t) (pos.x * SCALE + 0.5), y = (int_least32_t) (pos.y * SCALE + 0.5);
            if (x >= textbox.x && x < textbox.x + textbox.w && y >= textbox.y && y < textbox.y + textbox.h) {
                last_hit = &textbox;
                cursor_mouse(textbox, style, &cursor, x, y, selecting);
            } else last_hit = NULL;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 pos = GetMousePosition();
            int_least32_t x = (int_least32_t) (pos.x * SCALE + 0.5), y = (int_least32_t) (pos.y * SCALE + 0.5);
            if (last_hit == &textbox) cursor_mouse(textbox, style, &cursor, x, y, true);
        }
        if (IsKeyPressed(KEY_PAGE_UP) || IsKeyPressedRepeat(KEY_PAGE_UP)) cursor_page_up(textbox, style, &cursor, selecting);
        if (IsKeyPressed(KEY_PAGE_DOWN) || IsKeyPressedRepeat(KEY_PAGE_DOWN)) cursor_page_down(textbox, style, &cursor, selecting);
        
        DrawRectangleLines(textbox.x, textbox.y, textbox.w, textbox.h, WHITE);
        EndDrawing();
    }

    // ExportImage(glyph_atlas->image, "glyph_atlas.jpg");
    
    texture_atlas_destroy(&glyph_atlas);
    CloseWindow();
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    return 0;
}