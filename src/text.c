#include "bane.h"

ARRAY_DEFINE(int_least32_t)

typedef enum { END_WRAP = 0, END_LF = 1, END_INPUT = 2 } LineEnd;
typedef struct {
    int_least32_tArray xs; // x coordinates of possible cursor positions relative to line start x
    LineEnd end; // cause of line end. E.g. line ending with \n means one less entry in xs and a cursor right after \n should display in next line
    size_t count; // codepoints in line
    size_t offset; // codepoints[offset] is first character in line
    int_least32_t y; // text baseline, not bounding box
} Line;

// FOR DEBUGGING
// static void print_line(Line line) {
//     size_t array_str_len = line.xs.count * 5;
//     char array_str[max(array_str_len + 1, (size_t) 3)];
//     if (line.xs.count > 0) {
//         snprintf(array_str, array_str_len, "[");
//         size_t offset = 1;
//         for (size_t i = 0; i < line.xs.count; i++){
//             snprintf(array_str + offset, array_str_len - offset, "%3i, ", line.xs.items[i]);
//             offset += 5;
//         }
//         snprintf(array_str + array_str_len - 1, 2, "]");
//     } else sprintf(array_str, "[]");
//     printf("Line(offset=%lu, count=%lu, baseline=%i, xs.count=%lu, xs.items=%s)\n", line.offset, line.count, line.y, line.xs.count, array_str);
// }

static Line line_create() { return (Line) { .xs = create_int_least32_tArray(32) }; }
static void line_destroy(Line line) { destroy_int_least32_tArray(&line.xs);}
static bool line_contains(Line line, size_t offset, bool pre_wrap) {
    size_t line_end = line.offset + line.count;
    return (offset >= line.offset &&
        (offset < line_end ||
            (offset == line_end && line.end != END_LF && (pre_wrap || line.end == END_INPUT))
        )
    );
}

static bool locs_overlap(CursorLoc l1, CursorLoc l2) { return l1.offset == l2.offset && l1.pre_wrap == l2.pre_wrap; }
static bool locs_reversed(CursorLoc l1, CursorLoc l2) { return l1.offset > l2.offset || (l1.offset == l2.offset && l2.pre_wrap); }
static CursorLoc loc_first(CursorLoc l1, CursorLoc l2) { return locs_reversed(l1, l2) ? l2 : l1; }
static CursorLoc loc_last(CursorLoc l1, CursorLoc l2) { return locs_reversed(l1, l2) ? l1 : l2; }

// update selection and return whether it changed from active to inactive
static bool sel_deactivated(Cursor *cursor, bool selecting) {
    if (cursor->sel_active) cursor->sel_active = !locs_overlap(cursor->loc, cursor->sel_start);
    if (selecting) {
        if (!cursor->sel_active) {
            cursor->sel_active = true;
            cursor->sel_start = cursor->loc;
        }
    } else {
        if (cursor->sel_active) {
            cursor->sel_active = false;
            return true;
        }
    }
    return false;
}

// Shapes exactly one line worth of text. Only populates out's .xs, .count .end
static void shape_text(const uint32_t *codepoints, size_t max_len, FT_Face face, int_least32_t w, Line *out) {
    out->xs.count = 0;
    append_int_least32_t(&out->xs, 0);
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
            out->xs.count = count + 1;
            out->end = END_WRAP;
            out->count = count;
            return;
        } else append_int_least32_t(&out->xs, rounded_pen_x);
        if (cp == SPACE) last_space = i + 1;
    }
    out->count = i;
}

// NOTE different to CSS behavior, where line-height is multiplier of font-size.
// Here it is multiplier of computed baseline-to-baseline distance.
// Deep dive CSS: font metrics, line-height and vertical-align: https://iamvdo.me/en/blog/css-font-metrics-line-height-and-vertical-align
static int_least32_t line_height_px(Style style) { return style.line_height * ((style.face->size->metrics.height + 32) >> 6); }
static int_least32_t baseline_offset(FT_Face face) { // property not directly exposed by face object
    int_least32_t line_gap = face->size->metrics.height - (face->size->metrics.ascender - face->size->metrics.descender);
    return (face->size->metrics.ascender + (line_gap / 2) + 32) >> 6;
}

// Populates out with all attributes of Line covering the position `position, pre_wrap` in the text
static void shape_line(TextBox box, Style style, size_t offset, bool pre_wrap, Line *out) {
    int_least32_t line_height = line_height_px(style);
    out->y = box.y + baseline_offset(style.face);
    size_t processed = 0;
    while (true) {
        assert(processed <= box.codepoint_count);
        shape_text(box.codepoints + processed, box.codepoint_count - processed, style.face, box.w, out);
        out->offset = processed;
        processed += out->count;
        if (line_contains(*out, offset, pre_wrap)) break;
        else out->y += line_height;
    }
}

// populates out with line after in. Returns whether there is a next line. Does not modify out if there isn't a line after this!
static bool next_line(TextBox box, Style style, Line *in, Line *out) {
    size_t new_offset = in->offset + in->count;
    if (new_offset > box.codepoint_count || (new_offset == box.codepoint_count && in->end != END_LF)) return false; // no line after this
    out->y = in->y + line_height_px(style);
    out->offset = new_offset;
    shape_text(box.codepoints + out->offset, box.codepoint_count - new_offset, style.face, box.w, out);
    return true;
}

void draw_text(TextBox box, Style style) {
    // NOTE: characters aren't drawn one by one because it takes rendering the whole line in advance to know where it wraps
    Line line = line_create();
    shape_line(box, style, 0, true, &line); // first line
    FT_GlyphSlot slot = style.face->glyph; // shorthand
    while (true) {
        for (size_t i = 0; i < line.count; i++) {;
            uint32_t cp = box.codepoints[line.offset + i];
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
                // NOTE: not unloading `Image texture` because all it would do is free the freetype bitmap buffer. not intended.
            }
            tastatus = texture_atlas_draw(style.atlas, glyph_index, box.x + line.xs.items[i], line.y, style.text_color);
            assert(tastatus == TA_OK);
        }
        if (line.y + line_height_px(style) > box.y + box.h || !next_line(box, style, &line, &line)) break;
    }
    line_destroy(line);
}

static int_least32_t x_offset(Line line, size_t offset) {
    // assumes line has an x for offset, use line_contains to check or use line from shape_line
    if (line.offset == offset) return 0;
    assert(offset > line.offset);
    size_t idx = offset - line.offset;
    assert(line.xs.count > idx);
    return line.xs.items[idx];
}

static int_least32_t sel_min_w(Line line, size_t start, size_t end) { return line.count == 1 && line.end == END_LF ? 4 : 0; }

static void draw_selection(TextBox box, Style style, const Cursor *cursor) {
    if (!cursor->sel_active || locs_overlap(cursor->loc, cursor->sel_start)) return;
    assert(cursor->loc.offset <= box.codepoint_count && cursor->sel_start.offset <= box.codepoint_count);
    CursorLoc start, end;
    if (locs_reversed(cursor->loc, cursor->sel_start)) { start = cursor->sel_start; end = cursor->loc;}
    else { start = cursor->loc; end = cursor->sel_start; }
    Line line = line_create();
    shape_line(box, style, start.offset, start.pre_wrap, &line);
    int_least32_t x = box.x + x_offset(line, start.offset), y = line.y - baseline_offset(style.face), w, h = line_height_px(style);
    if (line_contains(line, end.offset, end.pre_wrap)) {
        DrawRectangle(x, y, max(sel_min_w(line, start.offset, end.offset), box.x + x_offset(line, end.offset) - x), h, style.selection_color);
    }
    else {
        DrawRectangle(x, y, max(sel_min_w(line, start.offset, end.offset), box.x + line.xs.items[line.xs.count - 1] - x), h, style.selection_color);
        while (true) {
            bool one_more_line = next_line(box, style, &line, &line);
            assert(one_more_line);
            if (line_contains(line, end.offset, end.pre_wrap)) break;
            else {
                w = max(sel_min_w(line, start.offset, end.offset), line.xs.items[line.xs.count - 1]);
                DrawRectangle(box.x, line.y - baseline_offset(style.face), w, h, style.selection_color);
            }
        }
        DrawRectangle(box.x, line.y - baseline_offset(style.face), x_offset(line, end.offset), h, style.selection_color);
    }
    line_destroy(line);
}

typedef struct { int_least32_t x, y, h; } CursorPos;

static CursorPos cursor_pos(TextBox box, Style style, const Cursor *cursor) {
    assert(cursor->loc.offset <= box.codepoint_count);
    CursorPos pos = { .x = box.x, .y = box.y, .h = line_height_px(style) };
    if (cursor->loc.offset > 0) {
        Line line = line_create();
        shape_line(box, style, cursor->loc.offset, cursor->loc.pre_wrap, &line);
        pos.x += x_offset(line, cursor->loc.offset);
        pos.y = line.y - baseline_offset(style.face);
        line_destroy(line);
    }
    return pos;
}

void draw_cursor(TextBox box, Style style, Cursor *cursor) {
    draw_selection(box, style, cursor);
    CursorPos pos = cursor_pos(box, style, cursor);
    DrawLineEx((Vector2) {pos.x, pos.y}, (Vector2) {pos.x, pos.y + pos.h}, 1, style.cursor_color);
}

void cursor_right(TextBox box, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_last(cursor->loc, cursor->sel_start);
    else {
        cursor->loc.offset = min(box.codepoint_count, cursor->loc.offset + 1);
        cursor->loc.pre_wrap = true;
        cursor->update_sticky_x = true;
    }
}

void cursor_left(Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_first(cursor->loc, cursor->sel_start);
    else {
        if (cursor->loc.offset > 0) cursor->loc.offset--;
        cursor->loc.pre_wrap = false;
        cursor->update_sticky_x = true;
    }
}

static void cursor_to_closest_x(TextBox box, Line line, Cursor *cursor, int_least32_t x) {
    cursor->loc.offset = line.offset;
    cursor->loc.pre_wrap = false;
    uint_least32_t d = abs(box.x - x);
    for (size_t i = 1; i < line.xs.count; i++) { // could be binary search for speed. starts at i == 1 because line.xs.items[0] = 0
        uint_least32_t d0 = abs(box.x + line.xs.items[i] - x);
        if (d0 < d) {
            d = d0;
            cursor->loc.offset = line.offset + i;
            cursor->loc.pre_wrap = true;
        }
    }
}

static void update_sticky_x(TextBox box, Style style, Cursor *cursor) {
    if (cursor->update_sticky_x) {
        cursor->loc.sticky_x = cursor_pos(box, style, cursor).x;
        cursor->update_sticky_x = false;
    }
}

void cursor_down(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_last(cursor->loc, cursor->sel_start);
    if (cursor->loc.offset < box.codepoint_count) {
        update_sticky_x(box, style, cursor);
        Line line = line_create();
        shape_line(box, style, cursor->loc.offset, cursor->loc.pre_wrap, &line);
        if (!next_line(box, style, &line, &line)) cursor->loc.offset = line.offset + line.count;
        else cursor_to_closest_x(box, line, cursor, cursor->loc.sticky_x);
        line_destroy(line);
    } else cursor->update_sticky_x = true;
}

void cursor_up(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_first(cursor->loc, cursor->sel_start);
    update_sticky_x(box, style, cursor);
    if (cursor->loc.offset > 0) {
        Line lines[2] = {line_create(), line_create()};
        uint_least8_t head = 0;
        shape_line(box, style, 0, true, &lines[head]);
        while(!line_contains(lines[head], cursor->loc.offset, cursor->loc.pre_wrap)) {
            bool one_more_line = next_line(box, style, lines + head, lines + ((head + 1) % 2));
            assert(one_more_line);
            head = (head + 1) % 2;
        }
        if (lines[head].offset == 0) { // no previous line
            cursor->loc.pre_wrap = false;
            cursor->loc.offset = 0;
        } else cursor_to_closest_x(box, lines[abs(head - 1) % 2], cursor, cursor->loc.sticky_x);
        line_destroy(lines[0]);
        line_destroy(lines[1]);
    } else cursor->update_sticky_x = true;
}

void cursor_home(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_first(cursor->loc, cursor->sel_start);
    if (cursor->loc.offset > 0) {
        Line line = line_create();
        shape_line(box, style, cursor->loc.offset, cursor->loc.pre_wrap, &line);
        if (cursor->loc.offset == line.offset) {
            for (; cursor->loc.offset > 0; cursor->loc.offset--) if (box.codepoints[cursor->loc.offset - 1] == LF) break;
        } else cursor->loc.offset = line.offset;
        line_destroy(line);
    }
    cursor->loc.pre_wrap = false;
    cursor->update_sticky_x = true;
}

void cursor_end(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_last(cursor->loc, cursor->sel_start);
    if (cursor->loc.offset < box.codepoint_count) {
        Line line = line_create();
        shape_line(box, style, cursor->loc.offset, cursor->loc.pre_wrap, &line);
        size_t line_end = line.offset + line.xs.count - 1;
        if (cursor->loc.offset == line_end) {
            for (; cursor->loc.offset < box.codepoint_count; cursor->loc.offset++) if (box.codepoints[cursor->loc.offset] == LF) break;
        } else cursor->loc.offset = line_end;
        line_destroy(line);
    }
    cursor->loc.pre_wrap = true;
    cursor->update_sticky_x = true;
}

void cursor_mouse(TextBox box, Style style, Cursor *cursor, Vector2 pos, bool selecting) {
    sel_deactivated(cursor, selecting);
    Line lines[2] = {line_create(), line_create()};
    uint_least8_t head = 0;
    shape_line(box, style, 0, true, &lines[head]);
    int_least32_t x = (int_least32_t) (pos.x * SCALE + 0.5), y = (int_least32_t) (pos.y  * SCALE + 0.5);
    int_least32_t dy = abs(box.y - y), h = line_height_px(style);
    while (true) {
        int_least32_t line_top = lines[head].y - baseline_offset(style.face);
        if (y >= line_top && y < line_top + h) break;
        else {
            int_least32_t dy1 = min(abs(line_top - y), abs(line_top + h - y));
            if (dy1 <= dy) dy = dy1;
            else break; // following lines only increase distance
        }
        if (!next_line(box, style, lines + head, lines + ((head + 1) % 2))) break;
        head = (head + 1) % 2;
    }
    cursor_to_closest_x(box, lines[head], cursor, x);
    line_destroy(lines[0]);
    line_destroy(lines[1]);
    cursor->update_sticky_x = true;
}