#include <stdio.h>
#include <math.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "bane.h"
#include "firasans_regular.c"

#define SCALE 1.25 // HACK: known scaling on testing machine
#define LF 0xA // \n
#define SPACE 0x20

ARRAY_DECLARE(int_least32_t)
ARRAY_DEFINE(int_least32_t)

typedef struct {
    int_least32_tArray xs; // x positions between characters, excluding before first character, including after last one.
    bool wrap;
    size_t codepoint_count;
} Line;

typedef struct {
    int_least32_t x, y, w, h;
    uint32_t *codepoints;
    size_t codepoint_count;
} TextBox;

typedef struct {
    size_t offset;
    bool pre_wrap;
    int_least32_t x;
} Cursor;

bool cursor_eq(Cursor c1, Cursor c2) { return c1.offset == c2.offset && c1.pre_wrap == c2.pre_wrap; }

typedef struct {
    Cursor start, end;
    bool active;
} Selection;

bool sel_is_reversed(Selection sel) {
    return sel.start.offset > sel.end.offset || (sel.start.offset == sel.end.offset && sel.end.pre_wrap);
}

Cursor sel_right(Selection sel) { return sel_is_reversed(sel) ? sel.start : sel.end; }
Cursor sel_left(Selection sel) { return sel_is_reversed(sel) ? sel.end : sel.start; }

void shapeText(const uint32_t *codepoints, size_t max_len, FT_Face face, int_least32_t w, Line *out) {
    out->xs.count = 0;
    out->wrap = false;
    // 64ths of a pixel to prevent accumulating error from adding rounded advance widths
    uint_least64_t pen_x = 0; 
    // last_space counts characters up to and including the most recent space
    size_t i, last_space = 0;
    for (i = 0; i < max_len; i++) {
        uint32_t cp = codepoints[i];
        if (cp == LF) {
            out->codepoint_count = i + 1;
            return;
        }
        FT_Error error = FT_Load_Char(face, cp, FT_LOAD_BITMAP_METRICS_ONLY);
        assert(!error);
        pen_x += face->glyph->advance.x;
        int_least32_t rounded_pen_x = (pen_x + 32) >> 6; // round to nearest pixel
        if (rounded_pen_x > w) {
            size_t count = last_space == 0 ? i : last_space;
            out->xs.count = count;
            out->wrap = true;
            out->codepoint_count = count;
            return;
        } else append_int_least32_t(&out->xs, rounded_pen_x);
        if (cp == SPACE) last_space = i + 1;
    }
    out->codepoint_count = i;
}

void BaneDrawText(TextBox box, TextureAtlas *atlas, FT_Face face) {
    Color text_color = { 0xe3, 0x88, 0x64, 0xff };
    int_least32_t lineheight = (face->size->metrics.height + 32) >> 6;
    // Deep dive CSS: font metrics, line-height and vertical-align: https://iamvdo.me/en/blog/css-font-metrics-line-height-and-vertical-align
    int_least32_t line_x = box.x, line_y = box.y + ((face->size->metrics.ascender + 32) >> 6);
    FT_GlyphSlot slot = face->glyph; // shorthand
    Line line = { .xs = create_int_least32_tArray(32) };
    size_t processed = 0;
    // printf("\n");
    while (processed < box.codepoint_count) {
        shapeText(box.codepoints + processed, box.codepoint_count - processed, face, box.w, &line);

        // printf("LINE: y=%3i, processed=%3lu, new_processed=%2lu, wrap=%i, [", line_y, processed, new_processed, line.wrap);
        // for (uint i = 0; i < line.xs.count; i++) {
        //     printf("%3i, ", line.xs.items[i]);
        // }
        // printf("]\n");

        assert(line.codepoint_count != 0); // would loop infinitely
        for (size_t i = 0; i < line.codepoint_count; i++) {
            uint32_t cp = *(box.codepoints + processed + i);
            if (cp == LF || cp == SPACE) continue;
            FT_UInt glyph_index = FT_Get_Char_Index(face, cp);
            TextureRect rect;
            TAStatus tastatus = texture_atlas_get_rect(&rect, atlas, glyph_index);
            if (tastatus == TA_RECT_NOT_FOUND) {
                FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
                assert(!error);
                assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported
                Image texture = {(void *) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
                tastatus = texture_atlas_add_get_rect(&rect, atlas, glyph_index, texture, slot->bitmap_left, slot->bitmap_top);
                assert(tastatus == TA_OK);
            }
            tastatus = texture_atlas_draw(atlas, glyph_index, line_x + line.xs.items[i] - line.xs.items[0], line_y, text_color);
            assert(tastatus == TA_OK);
        }
        processed += line.codepoint_count;
        line_y += lineheight;
        if (line_y > box.y + box.h) return;
    }
    destroy_int_least32_tArray(&line.xs);
}

typedef struct {
    int_least32_t x, y, h;
} CursorPos;

void DrawCursor(CursorPos pos) { DrawLineEx((Vector2) {pos.x, pos.y}, (Vector2) {pos.x, pos.y + pos.h}, 1, WHITE); }

CursorPos GetCursorPosition(TextBox box, FT_Face face, Cursor cursor) {
    assert(cursor.offset <= box.codepoint_count);
    int_least32_t lineheight = (face->size->metrics.height + 32) >> 6;
    int_least32_t cursor_x = box.x , cursor_y = box.y;
    size_t line_offset = 0;
    Line line = { .xs = create_int_least32_tArray(32), .codepoint_count = 0 };
    if (cursor.offset > 0) {
        while (true) {
            line_offset += line.codepoint_count;
            shapeText(box.codepoints + line_offset, box.codepoint_count - line_offset, face, box.w, &line);
            if (line_offset + line.codepoint_count < cursor.offset) cursor_y += lineheight;
            else break;
        }
        if (cursor.offset - line_offset > 0) {
            if (cursor.offset - line_offset > line.xs.count || // cursor is at end of a line that ends with \n and so has one less xs than number of characters
            (cursor.offset - line_offset == line.xs.count && !cursor.pre_wrap && line.wrap)) { // cursor is at end of a wrapping line but should be displayed in the next (pre_wrap = false)
                cursor_y += lineheight;
            } else cursor_x += line.xs.items[cursor.offset - line_offset - 1];  
        } 
    }
    destroy_int_least32_tArray(&line.xs);
    return (CursorPos) { cursor_x, cursor_y, ((face->size->metrics.ascender - face->size->metrics.descender + 32) >> 6) };
}

void DrawSelection(TextBox box, FT_Face face, Selection sel) {
    Color sel_color = { 255, 0, 0, 128 };
    if (cursor_eq(sel.start, sel.end) || !sel.active) { return; }
    assert(sel.start.offset <= box.codepoint_count && sel.end.offset <= box.codepoint_count);
    int_least32_t lineheight = (face->size->metrics.height + 32) >> 6;
    int_least32_t line_y = box.y;
    Cursor first, second;
    if (sel_is_reversed(sel)) { first = sel.end; second = sel.start; }
    else { first = sel.start; second = sel.end; }
    size_t line_offset, line_end_offset = 0;
    int_least32_t sel_x0, sel_x1;
    Line line = { .xs = create_int_least32_tArray(32), .codepoint_count = 0 };
    while (true) {
        line_offset = line_end_offset;
        shapeText(box.codepoints + line_offset, box.codepoint_count - line_offset, face, box.w, &line);
        line_end_offset = line_offset + line.codepoint_count;
        if ((line_end_offset > first.offset && line_offset <= second.offset) ||
        // include wrapping lines where selection starts at very end to be able to indicate that
        (line_end_offset == first.offset && line.wrap && first.pre_wrap)) {
            if (line.xs.count == 0) { // happens in lines with only \n
                sel_x0 = box.x;
                sel_x1 = sel_x0 + 4;
            }
            else {    
                if (line_offset >= first.offset) { sel_x0 = box.x; }
                else {
                    sel_x0 = box.x + line.xs.items[first.offset - line_offset - 1];
                }
                if (line_end_offset <= second.offset) {
                    // selection goes all the way to line end.
                    sel_x1 = box.x + line.xs.items[line.xs.count - 1];
                } else {
                    // selection ends midline
                    sel_x1 = box.x + line.xs.items[second.offset - line_offset - 1];
                }
            }
            DrawRectangle(sel_x0, line_y, max(sel_x1 - sel_x0, 4), lineheight, sel_color);
        }
        if (line_end_offset < second.offset) line_y += lineheight;
        else break;
    }
    destroy_int_least32_tArray(&line.xs);
}

void cursor_right(TextBox box, Cursor *cursor) {
    if (cursor->offset < box.codepoint_count) {
        cursor->offset++;
        cursor->pre_wrap = true;
    }
}

void cursor_left(Cursor *cursor) {
    if (cursor->offset > 0) {
        cursor->offset--;
        cursor->pre_wrap = false;
    }
}

void cursor_down(TextBox box, FT_Face face, Cursor *cursor) {
    assert(cursor->offset <= box.codepoint_count);
    size_t line_offset = 0;
    Line line = { .xs = create_int_least32_tArray(32), .codepoint_count = 0 };
    while (true) {
        line_offset += line.codepoint_count;
        shapeText(box.codepoints + line_offset, box.codepoint_count - line_offset, face, box.w, &line);
        if (line_offset + line.codepoint_count == box.codepoint_count ||
            line_offset + line.codepoint_count > cursor->offset ||
            (line_offset + line.codepoint_count == cursor->offset && line.wrap && cursor->pre_wrap)) break;
    }
    /*
    is there a line below?
        yes: which is the closest x to cursor.x?
        no: replace this movement with cursor_end / or go to end of line directly
    */
    size_t line_end_offset = line_offset + line.codepoint_count;
    // NOTE: if line is \n and its the last line, follwing two if statements will be false, so setting pre_wrap and offset here
    cursor->pre_wrap = false;
    cursor->offset = line_end_offset;
    if (line_end_offset < box.codepoint_count) { // is there at least one more line that can be shaped?
        shapeText(box.codepoints + line_end_offset, box.codepoint_count - line_end_offset, face, box.w, &line);
        uint_least32_t d = abs(box.x - cursor->x);
        for (size_t i = 0; i < line.xs.count; i++) { // could be binary search for speed
            uint_least32_t d0 = abs(box.x + line.xs.items[i] - cursor->x);
            if (d0 < d) {
                d = d0;
                cursor->offset = line_end_offset + i + 1;
                cursor->pre_wrap = true;
            }
        }
        printf("lines below, moving down\n");
    } else if (line.xs.count > 0) { // no lines below, move to end of line
        printf("no lines below, moving to end of line\n");
        cursor->pre_wrap = true;
        cursor->offset = line_end_offset;
    }
    destroy_int_least32_tArray(&line.xs);
}

void line_copy(Line *dest, Line src) {
    dest->codepoint_count = src.codepoint_count;
    dest->wrap = src.wrap;
    copy_int_least32_t(&dest->xs, &src.xs);
}

void cursor_up(TextBox box, FT_Face face, Cursor *cursor) {
    assert(cursor->offset <= box.codepoint_count);
    size_t line_offset = 0;
    bool prev_line_set = false;
    Line prev_line = { .xs = create_int_least32_tArray(32), .codepoint_count = 0 };
    Line line = { .xs = create_int_least32_tArray(32), .codepoint_count = 0 };
    while (true) {
        line_offset += line.codepoint_count;
        shapeText(box.codepoints + line_offset, box.codepoint_count - line_offset, face, box.w, &line);
        if (line_offset + line.codepoint_count == box.codepoint_count ||
            line_offset + line.codepoint_count > cursor->offset ||
            (line_offset + line.codepoint_count == cursor->offset && line.wrap && cursor->pre_wrap)) break;
        line_copy(&prev_line, line);
        prev_line_set = true;
    }
    /*
    is there a line above?
        yes: which is the closest x to cursor.x?
        no: replace this movement with cursor_start / or go to start of line directly
    */
    if (prev_line_set) {
        size_t line_start_offset = line_offset - prev_line.codepoint_count;
        printf("there is a previous line. start_offset = %lu\n", line_start_offset);
        cursor->offset = line_start_offset;
        cursor->pre_wrap = false;
        uint_least32_t d = abs(box.x - cursor->x);
        for (size_t i = 0; i < prev_line.xs.count; i++) { // could be binary search for speed
            uint_least32_t d0 = abs(box.x + prev_line.xs.items[i] - cursor->x);
            if (d0 < d) {
                d = d0;
                cursor->offset = line_start_offset + i + 1;
                cursor->pre_wrap = true;
            }
        }
    } else {
        printf("no previous line\n");
        cursor->offset = 0;
        cursor->pre_wrap = false;
    }
    destroy_int_least32_tArray(&line.xs);
    destroy_int_least32_tArray(&prev_line.xs);
}

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
    char *text = u8"\na\naaaaa\n\na";
    size_t len, processed_bytes;
    
    if (utf8_measure_codepoints(text, strlen(text), &len, &processed_bytes) != UTF8_OK) { return 1; }
    uint32_t codepoints[len];
    if (utf8_decode(text, strlen(text), true, len, codepoints, &len) != UTF8_OK) { return 1; }
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "Bane of my existence");

    Cursor cursor = { .offset = 0, .pre_wrap = false, .x = 0 };

    TextBox textbox = { .x = 20, .y = 0, .w = 50, .h = 600, .codepoints = codepoints, .codepoint_count = len };

    Selection sel = { .active = false };
    
    bool change_cursor_x = false;
    while (!WindowShouldClose()) {
        // CursorPos cursorpos = { 0 };
        // bool cursorpos_defined = false;
        BeginDrawing();
        ClearBackground(BLACK);
        BaneDrawText(textbox, glyph_atlas, face);
        bool selecting = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (IsKeyPressed(KEY_RIGHT)) {
            if (selecting) {
                if (sel.active) {
                    cursor_right(textbox, &cursor);
                    sel.active = !cursor_eq(sel.start, cursor);
                } else {
                    sel.start = cursor;
                    cursor_right(textbox, &cursor);
                    sel.active = true;
                }
                sel.end = cursor;
            } else {
                if (sel.active) {
                    cursor = sel_right(sel);
                    sel.active = false;
                } else cursor_right(textbox, &cursor);
            }
            change_cursor_x = true;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            if (selecting) {
                if (sel.active) {
                    cursor_left(&cursor);
                    sel.active = !cursor_eq(sel.start, cursor);
                } else {
                    sel.start = cursor;
                    cursor_left(&cursor);
                    sel.active = true;
                }
                sel.end = cursor;
            } else {
                if (sel.active) {
                    cursor = sel_left(sel);
                    sel.active = false;
                } else cursor_left(&cursor);
            }
            change_cursor_x = true;
        }
        if (IsKeyPressed(KEY_DOWN)) {
            if (selecting) {
                if (sel.active) {
                    cursor_down(textbox, face, &cursor);
                    sel.active = !cursor_eq(sel.start, cursor);
                } else {
                    sel.start = cursor;
                    cursor_down(textbox, face, &cursor);
                    sel.active = true;
                }
                sel.end = cursor;
            } else {
                if (sel.active) {
                    cursor = sel_right(sel);
                    sel.active = false;
                }
                cursor_down(textbox, face, &cursor);
            }
        }
        if (IsKeyPressed(KEY_UP)) {
            if (selecting) {
                if (sel.active) {
                    cursor_up(textbox, face, &cursor);
                    sel.active = !cursor_eq(sel.start, cursor);
                } else {
                    sel.start = cursor;
                    cursor_up(textbox, face, &cursor);
                    sel.active = true;
                }
                sel.end = cursor;
            } else {
                if (sel.active) {
                    cursor = sel_left(sel);
                    sel.active = false;
                }
                cursor_up(textbox, face, &cursor);
            }
        }
        DrawSelection(textbox, face, sel);
        // cursor.x = cursorpos.x;
        CursorPos cursor_pos = GetCursorPosition(textbox, face, cursor);
        if (change_cursor_x) {
            cursor.x = cursor_pos.x;
            change_cursor_x = false;
        }
        DrawCursor(cursor_pos);
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