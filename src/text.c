#include "bane.h"

ARRAY_DEFINE(int_least32_t)

typedef enum { END_WRAP = 0, END_LF = 1, END_INPUT = 2 } LineEnd;

typedef struct {
    int_least32_tArray xs; // possible cursor x positions in line, excluding before first character, which would always be 0
    LineEnd end;
    size_t count; // codepoints in line
    size_t offset; // codepoints[offset] is first character in line
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
    size_t array_str_len = line.xs.count * 5;
    char array_str[max(array_str_len + 1, (size_t) 3)];
    if (line.xs.count > 0) {
        snprintf(array_str, array_str_len, "[");
        size_t offset = 1;
        for (size_t i = 0; i < line.xs.count; i++){
            snprintf(array_str + offset, array_str_len - offset, "%3i, ", line.xs.items[i]);
            offset += 5;
        }
        snprintf(array_str + array_str_len - 1, 2, "]");
    } else sprintf(array_str, "[]");
    printf("Line(offset=%lu, count=%lu, baseline=%i, xs.count=%lu, xs.items=%s)\n", line.offset, line.count, line.y, line.xs.count, array_str);
}

bool locations_overlap(TextLocation l1, TextLocation l2) { return l1.offset == l2.offset && l1.pre_wrap == l2.pre_wrap; }
bool locations_reversed(TextLocation l1, TextLocation l2) { return l1.offset > l2.offset || (l1.offset == l2.offset && l2.pre_wrap); }
TextLocation location_first(TextLocation l1, TextLocation l2) { return locations_reversed(l1, l2) ? l2 : l1; }
TextLocation location_last(TextLocation l1, TextLocation l2) { return locations_reversed(l1, l2) ? l1 : l2; }

bool process_selection(Cursor *cursor, bool selecting, bool exit_at_start, bool move_after_deselect) {
    if (cursor->sel_active) cursor->sel_active = !locations_overlap(cursor->sel_start, cursor->cursor);
    if (selecting) {
        if (!cursor->sel_active) {
            cursor->sel_active = true;
            cursor->sel_start = cursor->cursor;
        }
    } else {
        if (cursor->sel_active) {
            cursor->cursor = exit_at_start ? location_first(cursor->cursor, cursor->sel_start) : location_last(cursor->cursor, cursor->sel_start);
            cursor->sel_active = false;
            return move_after_deselect;
        }
    }
    return true;
}

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

int_least32_t get_cursor_x(Line line, size_t offset) {
    if (line.offset == offset) return 0;
    assert(offset - line.offset > 0);
    size_t idx = offset - line.offset - 1;
    assert(line.xs.count > idx);
    return line.xs.items[idx];
}

void draw_selection(TextBox box, Style style, Cursor cursor) {
    if (!cursor.sel_active || locations_overlap(cursor.cursor, cursor.sel_start)) return;
    assert(cursor.cursor.offset <= box.codepoint_count && cursor.sel_start.offset <= box.codepoint_count);
    int_least32_t lineheight = line_height_px(style);
    TextLocation first, second;
    if (locations_reversed(cursor.cursor, cursor.sel_start)) { first = cursor.sel_start; second = cursor.cursor;}
    else { first = cursor.cursor; second = cursor.sel_start; }
    int_least32_t sel_x0, sel_x1;
    Line line = line_create();
    shape_line(box, style, first.offset, first.pre_wrap, &line);
    sel_x0 = box.x + get_cursor_x(line, first.offset);
    bool ends_in_same_line = line_contains_pos(line, second.offset, second.pre_wrap);
    sel_x1 = box.x + (ends_in_same_line ? get_cursor_x(line, second.offset) : line.xs.items[line.xs.count - 1]);
    DrawRectangle(sel_x0, line_top(line, style.face), line.xs.count == 0 ? 4 : sel_x1 - sel_x0, lineheight, style.selection_color);
    if (!ends_in_same_line) {
        while (true) {
            bool one_more_line = next_line(box, style, &line, &line);
            assert(one_more_line);
            if (line_contains_pos(line, second.offset, second.pre_wrap)) break;
            else DrawRectangle(box.x, line_top(line, style.face), line.xs.items[line.xs.count - 1], lineheight, style.selection_color);
        }
        sel_x0 = box.x;
        sel_x1 = box.x + get_cursor_x(line, second.offset);
        DrawRectangle(sel_x0, line_top(line, style.face), line.count == 0 ? 4 : sel_x1 - sel_x0, lineheight, style.selection_color);
    }
    line_destroy(line);
}

void draw_cursor(TextBox box, Style style, Cursor cursor) {
    draw_selection(box, style, cursor);
    DrawLineEx((Vector2) {cursor.x, cursor.y}, (Vector2) {cursor.x, cursor.y + cursor.h}, 1, style.cursor_color);
}

void set_cursor_position(TextBox box, Style style, Cursor *cursor) {
    assert(cursor->cursor.offset <= box.codepoint_count);
    cursor->x = box.x, cursor->y = box.y;
    if (cursor->cursor.offset > 0) {
        Line line = line_create();
        shape_line(box, style, cursor->cursor.offset, cursor->cursor.pre_wrap, &line);
        cursor->x = box.x + get_cursor_x(line, cursor->cursor.offset);
        cursor->y = line_top(line, style.face);
        line_destroy(line);
    }
    cursor->h = line_height_px(style);
}

void cursor_right(TextBox box, Cursor *cursor, bool selecting) {
    if (process_selection(cursor, selecting, false, false) && cursor->cursor.offset < box.codepoint_count) {
        cursor->cursor.offset++;
        cursor->cursor.pre_wrap = true;
    }
}    

void cursor_left(Cursor *cursor, bool selecting) {
    if (process_selection(cursor, selecting, true, false) && cursor->cursor.offset > 0) {
        cursor->cursor.offset--;
        cursor->cursor.pre_wrap = false;
    }
}

void cursor_set_closest_x(TextBox box, Line line, Cursor *cursor) {
    cursor->cursor.offset = line.offset;
    cursor->cursor.pre_wrap = false;
    uint_least32_t d = abs(box.x - cursor->cursor.sticky_x);
    for (size_t i = 0; i < line.xs.count; i++) { // could be binary search for speed
        uint_least32_t d0 = abs(box.x + line.xs.items[i] - cursor->cursor.sticky_x);
        if (d0 < d) {
            d = d0;
            cursor->cursor.offset = line.offset + i + 1;
            cursor->cursor.pre_wrap = true;
        }
    }
}

void cursor_down(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (process_selection(cursor, selecting, false, true) && cursor->cursor.offset < box.codepoint_count) {
        Line line = line_create();
        shape_line(box, style, cursor->cursor.offset, cursor->cursor.pre_wrap, &line);
        if (!next_line(box, style, &line, &line)) cursor->cursor.offset = line.offset + line.count;
        else cursor_set_closest_x(box, line, cursor);
        line_destroy(line);
    }
}

void cursor_up(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (process_selection(cursor, selecting, true, true) && cursor->cursor.offset > 0) {
        Line lines[2] = {line_create(), line_create()};
        uint_least8_t head = 0;
        shape_line(box, style, 0, true, &lines[head]);
        while(!line_contains_pos(lines[head], cursor->cursor.offset, cursor->cursor.pre_wrap)) {
            bool one_more_line = next_line(box, style, lines + head, lines + ((head + 1) % 2));
            assert(one_more_line);
            head = (head + 1) % 2;
        }
        if (lines[head].offset == 0) { // no previous line
            cursor->cursor.pre_wrap = false;
            cursor->cursor.offset = 0;
        } else cursor_set_closest_x(box, lines[abs(head - 1) % 2], cursor);
        line_destroy(lines[0]);
        line_destroy(lines[1]);
    }
}