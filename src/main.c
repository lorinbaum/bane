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

typedef enum { END_WRAP = 0, END_LF = 1, END_INPUT = 2 } LineEnd;

typedef struct {
    int_least32_tArray xs; // possible cursor x positions in line, excluding before first character, which would always be 0
    LineEnd end;
    size_t count; // codepoints in line
    size_t offset; // codepoint offset at start of line
    int_least32_t y; // baseline, not bounding box
} Line;

Line line_create() { return (Line) { .xs = create_int_least32_tArray(32) }; }
void line_destroy(Line line) { destroy_int_least32_tArray(&line.xs);}
bool line_contains_pos(Line line, size_t offset, bool pre_wrap) {
    size_t line_end = line.offset + line.count;
    return (offset >= line.offset &&
        (offset < line_end ||
            (offset == line_end && line.end != END_LF && (pre_wrap || line.end == END_INPUT))
        )
    );
}
int_least32_t line_top(Line line, FT_Face face) { return line.y - ((face->size->metrics.ascender + 32) >> 6); }
void print_line(Line line) {
    printf("Line(offset=%lu, count=%lu, baseline=%i, xs.count=%u)\n", line.offset, line.count, line.y, line.xs.count);
}

typedef struct {
    int_least32_t x, y, w, h;
    uint32_t *codepoints;
    size_t codepoint_count;
} TextBox;

typedef struct {
    size_t offset;
    bool pre_wrap;
    int_least32_t x_sticky, x, y, h; // x_sticky = x coordinate that should persist across vertical movement
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

void shape_text(const uint32_t *codepoints, size_t max_len, FT_Face face, int_least32_t w, Line *out) {
    // sets out.count, out.xs.count, out.end
    out->xs.count = 0;
    out->end = END_INPUT;
    uint_least64_t pen_x = 0; // 64ths of a pixel to prevent accumulating error from adding rounded advance widths
    size_t i, last_space = 0; // last_space counts characters up to and including the most recent space
    for (i = 0; i < max_len; i++) {
        uint32_t cp = codepoints[i];
        if (cp == LF) {
            out->count = i + 1;
            out->end = END_LF;
            return;
        }
        FT_Error error = FT_Load_Char(face, cp, FT_LOAD_BITMAP_METRICS_ONLY);
        assert(!error);
        pen_x += face->glyph->advance.x;
        int_least32_t rounded_pen_x = (pen_x + 32) >> 6; // round to nearest pixel
        if (rounded_pen_x > w) {
            size_t count = last_space == 0 ? i : last_space;
            out->xs.count = count;
            out->end = END_WRAP;
            out->count = count;
            return;
        } else append_int_least32_t(&out->xs, rounded_pen_x);
        if (cp == SPACE) last_space = i + 1;
    }
    out->count = i;
}

typedef struct {
    TextureAtlas *atlas;
    FT_Face face;
    uint_least32_t line_height;
    Color text_color, cursor_color, selection_color;
} Style;

int_least32_t line_height_px(Style style) { return style.line_height * ((style.face->size->metrics.height + 32) >> 6); }

void shape_line(TextBox box, Style style, size_t offset, bool pre_wrap, Line *out) {
    int_least32_t line_height = line_height_px(style);
    // Deep dive CSS: font metrics, line-height and vertical-align: https://iamvdo.me/en/blog/css-font-metrics-line-height-and-vertical-align
    out->y = box.y + ((style.face->size->metrics.ascender + 32) >> 6); // baseline, not bounding box
    size_t processed = 0;
    while (true) {
        assert(processed <= box.codepoint_count);
        shape_text(box.codepoints + processed, box.codepoint_count - processed, style.face, box.w, out);
        out->offset = processed;
        processed += out->count;
        if (!line_contains_pos(*out, offset, pre_wrap)) out->y += line_height;
        else break;
    }
}

bool next_line(TextBox box, Style style, Line *in, Line *out) {
    // does not modify out if there isn't a line after this!
    size_t new_offset = in->offset + in->count;
    if (new_offset > box.codepoint_count || (new_offset == box.codepoint_count && in->end != END_LF)) return false; // no line after this
    out->y += line_height_px(style);
    out->offset = new_offset;
    shape_text(box.codepoints + out->offset, box.codepoint_count - new_offset, style.face, box.w, out);
    return true;
}

void draw_text(TextBox box, Style style) {
    // NOTE: characters aren't drawn one by one because it takes rendering the whole line in advance to know where it wraps
    Line line = line_create();
    shape_line(box, style, 0, true, &line); // first line
    FT_GlyphSlot slot = style.face->glyph; // shorthand
    bool one_more_line = true;
    while (one_more_line) {
        for (size_t i = 0; i < line.count; i++) {
            uint32_t cp = *(box.codepoints + line.offset + i);
            if (cp == LF || cp == SPACE) continue;
            FT_UInt glyph_index = FT_Get_Char_Index(style.face, cp);
            TextureRect rect;
            TAStatus tastatus = texture_atlas_get_rect(&rect, style.atlas, glyph_index);
            if (tastatus == TA_RECT_NOT_FOUND) {
                FT_Error error = FT_Load_Glyph(style.face, glyph_index, FT_LOAD_RENDER);
                assert(!error);
                assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported
                Image texture = {(void *) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
                tastatus = texture_atlas_add_get_rect(&rect, style.atlas, glyph_index, texture, slot->bitmap_left, slot->bitmap_top);
                assert(tastatus == TA_OK);
            }
            tastatus = texture_atlas_draw(style.atlas, glyph_index, box.x + line.xs.items[i] - line.xs.items[0], line.y, style.text_color);
            assert(tastatus == TA_OK);
        }
        one_more_line = next_line(box, style, &line, &line);
        if (line.y > box.y + box.h) return;
    }
    line_destroy(line);
}

void DrawCursor(Cursor cursor) { DrawLineEx((Vector2) {cursor.x, cursor.y}, (Vector2) {cursor.x, cursor.y + cursor.h}, 1, WHITE); }

int_least32_t get_cursor_x(Line line, size_t offset) {
    if (line.offset == offset) return 0;
    assert(offset - line.offset > 0);
    size_t idx = offset - line.offset - 1;
    assert(line.xs.count > idx);
    return line.xs.items[idx];
}

void SetCursorPosition(TextBox box, Style style, Cursor *cursor) {
    assert(cursor->offset <= box.codepoint_count);
    cursor->x = box.x, cursor->y = box.y;
    if (cursor->offset > 0) {
        Line line = line_create();
        shape_line(box, style, cursor->offset, cursor->pre_wrap, &line);
        cursor->x = box.x + get_cursor_x(line, cursor->offset);
        cursor->y = line_top(line, style.face);
        line_destroy(line);
    }
    cursor->h = line_height_px(style);
}

void DrawSelection(TextBox box, Style style, Selection sel) {
    if (cursor_eq(sel.start, sel.end) || !sel.active) { return; }
    assert(sel.start.offset <= box.codepoint_count && sel.end.offset <= box.codepoint_count);
    int_least32_t lineheight = line_height_px(style);
    Cursor first, second;
    if (sel_is_reversed(sel)) { first = sel.end; second = sel.start; }
    else { first = sel.start; second = sel.end; }
    int_least32_t sel_x0, sel_x1;
    Line line = line_create();
    shape_line(box, style, first.offset, first.pre_wrap, &line);
    sel_x0 = get_cursor_x(line, first.offset);
    bool ends_in_same_line = line_contains_pos(line, second.offset, second.pre_wrap);
    sel_x1 = ends_in_same_line ? get_cursor_x(line, second.offset) : line.xs.items[line.xs.count - 1];
    DrawRectangle(sel_x0, line_top(line, style.face), max(sel_x1 - sel_x0, 4), lineheight, style.selection_color);

    if (!ends_in_same_line) {
        bool one_more_line = next_line(box, style, &line, &line);
        assert(one_more_line);
        while (true) {
            one_more_line = next_line(box, style, &line, &line);
            assert(one_more_line);
            if (line_contains_pos(line, second.offset, second.pre_wrap)) break;
            else DrawRectangle(box.x, line_top(line, style.face), line.xs.items[line.xs.count - 1], lineheight, style.selection_color);
        }
        sel_x0 = box.x;
        sel_x1 = get_cursor_x(line, second.offset);
        DrawRectangle(sel_x0, line_top(line, style.face), max(sel_x1 - sel_x0, 4), lineheight, style.selection_color);
    }
    line_destroy(line);
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

void cursor_set_closest_x(TextBox box, Line line, Cursor *cursor) {
    cursor->offset = line.offset;
    cursor->pre_wrap = false;
    uint_least32_t d = abs(box.x - cursor->x_sticky);
    for (size_t i = 0; i < line.xs.count; i++) { // could be binary search for speed
        uint_least32_t d0 = abs(box.x + line.xs.items[i] - cursor->x_sticky);
        if (d0 < d) {
            d = d0;
            cursor->offset = line.offset + i + 1;
            cursor->pre_wrap = true;
        }
    }
}

void cursor_down(TextBox box, Style style, Cursor *cursor) {
    Line line = line_create();
    shape_line(box, style, cursor->offset, cursor->pre_wrap, &line);
    if (!next_line(box, style, &line, &line)) cursor->offset = line.offset + line.count;
    else cursor_set_closest_x(box, line, cursor);
    line_destroy(line);
}

void cursor_up(TextBox box, Style style, Cursor *cursor) {
    if (cursor->offset > 0) {
        Line lines[2] = {line_create(), line_create()};
        uint_least8_t head = 0;
        shape_line(box, style, 0, true, &lines[head]);
        while(!line_contains_pos(lines[head], cursor->offset, cursor->pre_wrap)) {
            bool one_more_line = next_line(box, style, lines + head, lines + ((head + 1) % 2));
            assert(one_more_line);
            head = (head + 1) % 2;
        }
        if (lines[head].offset == 0) { // no previous line
            cursor->pre_wrap = false;
            cursor->offset = 0;
        } else cursor_set_closest_x(box, lines[abs(head - 1) % 2], cursor);
        line_destroy(lines[0]);
        line_destroy(lines[1]);
    }
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
    char *text = u8"\na\naaaaa\n\na\n";
    size_t len, processed_bytes;
    
    if (utf8_measure_codepoints(text, strlen(text), &len, &processed_bytes) != UTF8_OK) { return 1; }
    uint32_t codepoints[len];
    if (utf8_decode(text, strlen(text), true, len, codepoints, &len) != UTF8_OK) { return 1; }
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "Bane of my existence");

    Cursor cursor = { .offset = 0, .pre_wrap = false, .x = 0 };

    TextBox textbox = { .x = 20, .y = 0, .w = 50, .h = 600, .codepoints = codepoints, .codepoint_count = len };

    Selection sel = { .active = false };

    Style style = {
        .atlas = glyph_atlas,
        .cursor_color = WHITE,
        .face = face,
        .line_height = 1,
        .selection_color = (Color) { 255, 0, 0, 128 },
        .text_color = (Color) { 0xe3, 0x88, 0x64, 0xff }
    };
    
    bool change_cursor_x = false;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        draw_text(textbox, style);
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
                    cursor_down(textbox, style, &cursor);
                    sel.active = !cursor_eq(sel.start, cursor);
                } else {
                    sel.start = cursor;
                    cursor_down(textbox, style, &cursor);
                    sel.active = true;
                }
                sel.end = cursor;
            } else {
                if (sel.active) {
                    cursor = sel_right(sel);
                    sel.active = false;
                }
                cursor_down(textbox, style, &cursor);
            }
        }
        if (IsKeyPressed(KEY_UP)) {
            if (selecting) {
                if (sel.active) {
                    cursor_up(textbox, style, &cursor);
                    sel.active = !cursor_eq(sel.start, cursor);
                } else {
                    sel.start = cursor;
                    cursor_up(textbox, style, &cursor);
                    sel.active = true;
                }
                sel.end = cursor;
            } else {
                if (sel.active) {
                    cursor = sel_left(sel);
                    sel.active = false;
                }
                cursor_up(textbox, style, &cursor);
            }
        }
        DrawSelection(textbox, style, sel);
        SetCursorPosition(textbox, style, &cursor);
        if (change_cursor_x) {
            cursor.x_sticky = cursor.x;
            change_cursor_x = false;
        }
        DrawCursor(cursor);
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