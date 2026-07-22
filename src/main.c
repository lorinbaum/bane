#include <stdio.h>
#include <math.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "bane.h"

int main(void) {
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "Bane of my existence");
    
    char *text = u8"assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported\n"
    "Image texture = {(void *) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};\n"
    "status = texture_atlas_add_get_rect(&rect, atlas, glyph_index, texture, slot->bitmap_left, slot->bitmap_top);\n"
    "assert(status == TA_OK);\n\n"
    "assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported\na";
    
    TextBox textbox = textbox_create(50, 50, GetScreenWidth() - 100, 100, text);
    
    Context *context = ctx_create();
    Style style = {
        .font_size = 20,
        .cursor_color = WHITE,
        .line_height = 1,
        .selection_color = (Color) { 0xff, 0, 0, 0x60 },
        .text_color = (Color) { 0xe3, 0x88, 0x64, 0xff },
        .text_selected_color = (Color) { 0xe3, 0x88, 0x64, 0xff }
    };
    ctx_load_style(context, style);

    TextBox *last_hit = NULL;

    while (!WindowShouldClose()) {
        if (IsWindowResized()) textbox.w = GetScreenWidth() - 100;
        BeginDrawing();
        ClearBackground(BLACK);
        tb_draw(&textbox, context);
        bool selecting = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
            if (ctrl) tb_next_word(&textbox, selecting);
            else tb_right(&textbox, selecting);
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
            if (ctrl) tb_prev_word(&textbox, selecting);
            else tb_left(&textbox, selecting);
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) tb_down(&textbox, context, selecting); 
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) tb_up(&textbox, context, selecting);
        if (IsKeyPressed(KEY_HOME) || IsKeyPressedRepeat(KEY_HOME)) tb_home(&textbox, context, selecting);
        if (IsKeyPressed(KEY_END) || IsKeyPressedRepeat(KEY_END)) tb_end(&textbox, context, selecting);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 pos = GetMousePosition();
            int_least32_t x = (int_least32_t) (pos.x * SCALE + 0.5), y = (int_least32_t) (pos.y * SCALE + 0.5);
            if (tb_hit(textbox, x, y)) {
                last_hit = &textbox;
                tb_mouse(&textbox, context, x, y, selecting);
            } else last_hit = NULL;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 pos = GetMousePosition();
            int_least32_t x = (int_least32_t) (pos.x * SCALE + 0.5), y = (int_least32_t) (pos.y * SCALE + 0.5);
            if (last_hit == &textbox) tb_mouse(&textbox, context, x, y, true);
        }
        if (IsKeyPressed(KEY_PAGE_UP) || IsKeyPressedRepeat(KEY_PAGE_UP)) tb_page_up(&textbox, context, selecting);
        if (IsKeyPressed(KEY_PAGE_DOWN) || IsKeyPressedRepeat(KEY_PAGE_DOWN)) tb_page_down(&textbox, context, selecting);

        if (IsKeyPressed(KEY_C) && ctrl) tb_copy(textbox);
        if ((IsKeyPressed(KEY_V) || IsKeyPressedRepeat(KEY_V)) && ctrl) tb_paste(&textbox);
        if (IsKeyPressed(KEY_X) && ctrl) tb_cut(&textbox);
        
        if (IsKeyPressed(KEY_A) && ctrl) tb_select_all(&textbox);

        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) tb_backspace(&textbox);
        if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) tb_delete(&textbox);

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressedRepeat(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressedRepeat(KEY_KP_ENTER)) {
            tb_write(&textbox, LF);
        }
        uint32_t c = GetCharPressed();
        while (c) {
            tb_write(&textbox, c);
            c = GetCharPressed();
        }

        float mouse_wheel_move = GetMouseWheelMove();
        if (mouse_wheel_move) {
            Vector2 pos = GetMousePosition();
            int_least32_t x = (int_least32_t) (pos.x * SCALE + 0.5), y = (int_least32_t) (pos.y * SCALE + 0.5);
            if (tb_hit(textbox, x, y)) {
                textbox.scroll_y = min(0, textbox.scroll_y + (int_least32_t) (mouse_wheel_move * 160));
            }
        }

        
        DrawRectangleLines(textbox.x, textbox.y, textbox.w, textbox.h, WHITE);
        EndDrawing();
    }

    // ExportImage(glyph_atlas->image, "glyph_atlas.jpg");
    
    gb_destroy(&textbox.gb);
    ctx_destroy(context);
    CloseWindow();

    return 0;
}