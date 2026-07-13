#include "bane.h"

typedef enum {
    END_WRAP = 0, // Cursor at line end may jump to next line if pre_wrap is false
    END_LF = 1,   // line.xs holds one less value and cursor after \n should display in next line
    END_INPUT = 2 // Cursor at line end does not jump to next line regardless of pre_wrap
} LineEnd;

typedef struct {
    sb_int_least32_t xs;  // x offsets of possible cursor positions relative to x of left line edge
    LineEnd end;            // cause of line end
    size_t count;           // codepoints in line
    size_t offset;          // codepoints[offset] is first character in line
    int_least32_t y;        // text baseline, not bounding box
} Line;

// FOR DEBUGGING
// static void print_line(Line line) {
//     size_t array_str_len = line.xs.count * 5;
//     char array_str[max(array_str_len + 1, (size_t) 3)];
//     if (line.xs.count > 0) {
//         snprintf(array_str, array_str_len, "[");
//         size_t offset = 1;
//         for (size_t i = 0; i < line.xs.count; i++){
//             snprintf(array_str + offset, array_str_len - offset, "%3i, ", sb_get(line.xs, i);
//             offset += 5;
//         }
//         snprintf(array_str + array_str_len - 1, 2, "]");
//     } else sprintf(array_str, "[]");
//     printf("Line(offset=%lu, count=%lu, baseline=%i, xs.count=%lu, xs.items=%s)\n", line.offset, line.count, line.y, line.xs.count, array_str);
// }

static Line line_create() { return (Line) { .xs = sb_create(int_least32_t, 32) }; }
static void line_destroy(Line line) { sb_destroy(&line.xs);}
static bool line_contains(Line line, size_t offset, bool pre_wrap) {
    size_t end_offset = line.offset + line.count;
    return (offset >= line.offset &&
        (offset < end_offset ||
            (offset == end_offset && line.end != END_LF && (pre_wrap || line.end == END_INPUT))
        )
    );
}
static void line_copy(Line *dest, Line *src) {
    dest->offset = src->offset;
    dest->end = src->end;
    dest->count = src->count;
    dest->y = src->y;
    sb_copy(&dest->xs, &src->xs);
}

static bool locs_overlap(CursorLoc l1, CursorLoc l2) { return l1.offset == l2.offset && l1.pre_wrap == l2.pre_wrap; }
static bool locs_reversed(CursorLoc l1, CursorLoc l2) { return l1.offset > l2.offset || (l1.offset == l2.offset && l2.pre_wrap); }
static CursorLoc loc_first(CursorLoc l1, CursorLoc l2) { return locs_reversed(l1, l2) ? l2 : l1; }
static CursorLoc loc_last(CursorLoc l1, CursorLoc l2) { return locs_reversed(l1, l2) ? l1 : l2; }

// update selection and return whether it changed from active to inactive
static bool sel_deactivated(Cursor *cursor, bool selecting) {
    if (cursor->sel_active) cursor->sel_active = !locs_overlap(cursor->loc, cursor->sel_start);
    if (selecting) {
        if (!cursor->sel_active) cursor->sel_start = cursor->loc;
        cursor->sel_active = true;
        return false;
    } else {
        bool deactivated = cursor->sel_active;
        cursor->sel_active = false;
        return deactivated;
    }
}

// Shapes exactly one line worth of text. Only populates out's .xs, .count .end
static void shape_text(const uint32_t *codepoints, size_t max_len, FT_Face face, int_least32_t w, Line *out) {
    out->xs.count = 0;
    sb_append(&out->xs, 0);
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
        } else sb_append(&out->xs, rounded_pen_x);
        if (cp == SPACE) last_space = i + 1;
    }
    out->count = i;
}

// NOTE different to CSS behavior, where line-height is multiplier of font-size.
// Here it is multiplier of computed baseline-to-baseline distance.
// Deep dive CSS: font metrics, line-height and vertical-align: https://iamvdo.me/en/blog/css-font-metrics-line-height-and-vertical-align
static int_least32_t line_height_px(Style style) { return style.line_height * ((style.face->size->metrics.height + 32) >> 6); }

// property not directly accessible through FT_Face
static int_least32_t baseline_offset_px(FT_Face face) {
    int_least32_t line_gap = face->size->metrics.height - (face->size->metrics.ascender - face->size->metrics.descender);
    return (face->size->metrics.ascender + (line_gap / 2) + 32) >> 6;
}

static void first_line(TextBox box, FT_Face face, Line *out) {
    out->y = box.y + baseline_offset_px(face);
    out->offset = 0;
    shape_text(box.codepoints, box.codepoint_count, face, box.w, out);
}

// populates out with line after in. Returns whether there is a line after in. If there isn't out is not modified!
// NOTE: next_line is strongly assumed to successfully reach all valid positions in predictable system state.
// So: if next_line is expected, a pattern like "if (!next_line) abort()" is deemed acceptable
static bool next_line(TextBox box, Style style, Line *in, Line *out) {
    size_t new_offset = in->offset + in->count;
    if (new_offset > box.codepoint_count || (new_offset == box.codepoint_count && in->end != END_LF)) return false; // no line after in
    out->y = in->y + line_height_px(style);
    out->offset = new_offset;
    shape_text(box.codepoints + out->offset, box.codepoint_count - new_offset, style.face, box.w, out);
    return true;
}

// Populates out with all attributes of Line covering Position(offset, pre_wrap) in the text
static void shape_line(TextBox box, Style style, size_t offset, bool pre_wrap, Line *out) {
    assert(offset <= box.codepoint_count);
    first_line(box, style.face, out);
    while (!line_contains(*out, offset, pre_wrap)) ensure(next_line(box, style, out, out));
}

static int_least32_t x_offset(Line line, size_t offset) {
    // return entry from line.xs corresponding to offset. NOTE: Returned value is still be relative to line left edge.
    // To ensure line has corresponding x, use line_contains or use lines from shape_line
    if (line.offset == offset) return 0;
    assert(offset > line.offset);
    size_t idx = offset - line.offset;
    assert(line.xs.count > idx);
    return sb_get(line.xs, idx);
}

static void draw_char(FT_Face face, TextureAtlas *atlas, uint32_t cp, int_least32_t x, int_least32_t y, Color color) {
    if (cp == LF || cp == SPACE) return;
    FT_UInt glyph_index = FT_Get_Char_Index(face, cp);
    TextureRect rect;
    TaError ta_error = texture_atlas_get_rect(atlas, glyph_index, &rect);
    if (ta_error == TA_RECT_NOT_FOUND) {
        FT_GlyphSlot slot = face->glyph;
        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        assert(!error);
        assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported
        Image texture = {(void *) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
        ta_error = texture_atlas_add_get_rect(atlas, glyph_index, texture, slot->bitmap_left, slot->bitmap_top, &rect);
        assert(!ta_error);
        // NOTE: No UnloadImage(image) because all it would do is free the freetype bitmap buffer. not intended.
    }
    ta_error = texture_atlas_draw(atlas, glyph_index, x, y, color);
    assert(!ta_error);
}

void draw_text(TextBox *box, Style style, TextureAtlas *atlas, Cursor *cursor) {
    // NOTE: No clipping is performed. Characters that show partly inside the box also render outside it. Wait for WebGPU backend to fix efficiently
    // characters aren't drawn one by one because it takes preprocessing the whole line to know where it wraps
    Line line = line_create();
    int_least32_t cursor_x, cursor_y, line_h = line_height_px(style), baseline_offset = baseline_offset_px(style.face);
    if (cursor != NULL) {
        shape_line(*box, style, cursor->loc.offset, cursor->loc.pre_wrap, &line);
        cursor_x = box->x + x_offset(line, cursor->loc.offset);
        cursor_y = line.y - baseline_offset;
        if (cursor->scroll_to) { // scroll into view
            if (cursor_y + box->scroll_y < box->y) box->scroll_y = box->y - cursor_y;
            else if (cursor_y + line_h + box->scroll_y > box->y + box->h) box->scroll_y = box->y + box->h - (cursor_y + line_h);
            cursor->scroll_to = false;
        }
    }
    first_line(*box, style.face, &line);
    // skip invisible lines
    while (line.y - baseline_offset + line_h + box->scroll_y <= box->y) if (!next_line(*box, style, &line, &line)) return; 
    if (cursor != NULL && cursor->sel_active && !locs_overlap(cursor->loc, cursor->sel_start)) {
        // draw text + selection together
        CursorLoc start, end;
        if (locs_reversed(cursor->loc, cursor->sel_start)) { start = cursor->sel_start; end = cursor->loc;}
        else { start = cursor->loc; end = cursor->sel_start; }
        do {
            // draw text
            for (size_t i = 0; i < line.count; i++) {
                Color color = line.offset + i >= start.offset && line.offset + i < end.offset ? style.text_selected_color : style.text_color;
                draw_char(style.face, atlas, box->codepoints[line.offset + i], box->x + sb_get(line.xs, i), line.y + box->scroll_y, color);
            }
            // draw selection
            if (line_contains(line, start.offset, start.pre_wrap)) {
                int_least32_t x = box->x + x_offset(line, start.offset), y = line.y - baseline_offset + box->scroll_y, w;
                if (line_contains(line, end.offset, end.pre_wrap)) w = box->x + x_offset(line, end.offset) - x;
                else w = box->x + sb_rget(line.xs, 1) - x + (line.end == END_LF ? 4 : 0);
                DrawRectangle(x, y, w, line_h, style.selection_color);
            } else if (line.offset + line.count > start.offset && line.offset <= end.offset) {
                int_least32_t x = box->x, y = line.y - baseline_offset + box->scroll_y, w;
                if (line_contains(line, end.offset, end.pre_wrap)) w = box->x + x_offset(line, end.offset) - x;
                else w = sb_rget(line.xs, 1) + (line.end == END_LF ? 4 : 0);
                DrawRectangle(x, y, w, line_h, style.selection_color);
            }
        } while (line.y + line_h + box->scroll_y < box->y + box->h && next_line(*box, style, &line, &line));
    } else do for (size_t i = 0; i < line.count; i++) {
        // draw only text
        draw_char(style.face, atlas, box->codepoints[line.offset + i], box->x + sb_get(line.xs, i), line.y + box->scroll_y, style.text_color);
    } while (line.y + line_h + box->scroll_y < box->y + box->h && next_line(*box, style, &line, &line));
    line_destroy(line);
    // draw cursor. Drawn at end to layer over text and selection while avoiding raylib's rshapes which wants camera and 3D Mode.
    // Wait for WebGPU backend to fix.
    if (cursor != NULL && cursor_y + box->scroll_y >= box->y && cursor_y + box->scroll_y < box->y + box->h) {
        DrawLineEx((Vector2) {cursor_x, cursor_y + box->scroll_y}, (Vector2) {cursor_x, cursor_y + line_h + box->scroll_y}, 1, style.cursor_color);
    }
}

void cursor_right(TextBox box, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_last(cursor->loc, cursor->sel_start);
    else {
        if (cursor->loc.offset < box.codepoint_count) cursor->loc.offset++;
        cursor->loc.pre_wrap = true;
        cursor->update_sticky_x = true;
    }
    cursor->scroll_to = true;
}

void cursor_left(Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_first(cursor->loc, cursor->sel_start);
    else {
        if (cursor->loc.offset > 0) cursor->loc.offset--;
        cursor->loc.pre_wrap = false;
        cursor->update_sticky_x = true;
    }
    cursor->scroll_to = true;
}

// find closest entry in line.xs to x, set cursor to corresponding offset, pre_wrap
static void cursor_to_closest_x(TextBox box, Line line, Cursor *cursor, int_least32_t x) {
    cursor->loc.offset = line.offset;
    cursor->loc.pre_wrap = false;
    uint_least32_t d = labs(box.x - x);
    for (size_t i = 1; i < line.xs.count; i++) { // could be binary search for speed. starts at i == 1 because sb_get(line.xs, 0) = 0
        uint_least32_t d0 = labs(box.x + sb_get(line.xs, i) - x);
        if (d0 < d) {
            d = d0;
            cursor->loc.offset = line.offset + i;
            cursor->loc.pre_wrap = true;
        }
    }
}

// to call by functions that use cursor->loc.sticky_x. requires shaping lines, so only used when necessary
static void update_sticky_x(TextBox box, Style style, Cursor *cursor) {
    if (cursor->update_sticky_x) {
        Line line = line_create();
        shape_line(box, style, cursor->loc.offset, cursor->loc.pre_wrap, &line);
        cursor->loc.sticky_x = box.x + x_offset(line, cursor->loc.offset);
        cursor->update_sticky_x = false;
        line_destroy(line);
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
    cursor->scroll_to = true;
}

void cursor_up(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_first(cursor->loc, cursor->sel_start);
    if (cursor->loc.offset > 0) {
        update_sticky_x(box, style, cursor);
        Line lines[2] = {line_create(), line_create()};
        uint_least8_t head = 0;
        first_line(box, style.face, &lines[head]);
        while(!line_contains(lines[head], cursor->loc.offset, cursor->loc.pre_wrap)) {
            ensure(next_line(box, style, lines + head, lines + ((head + 1) % 2)));
            head = (head + 1) % 2;
        }
        if (lines[head].offset == 0) { // no previous line
            cursor->loc.pre_wrap = false;
            cursor->loc.offset = 0;
        } else cursor_to_closest_x(box, lines[labs(head - 1) % 2], cursor, cursor->loc.sticky_x);
        line_destroy(lines[0]);
        line_destroy(lines[1]);
    } else cursor->update_sticky_x = true;
    cursor->scroll_to = true;
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
    cursor->scroll_to = true;
}

void cursor_end(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_last(cursor->loc, cursor->sel_start);
    if (cursor->loc.offset < box.codepoint_count) {
        Line line = line_create();
        shape_line(box, style, cursor->loc.offset, cursor->loc.pre_wrap, &line);
        size_t last_offset = line.offset + line.xs.count - 1;
        if (cursor->loc.offset == last_offset) {
            for (; cursor->loc.offset < box.codepoint_count; cursor->loc.offset++) if (box.codepoints[cursor->loc.offset] == LF) break;
        } else cursor->loc.offset = last_offset;
        line_destroy(line);
    }
    cursor->loc.pre_wrap = true;
    cursor->update_sticky_x = true;
    cursor->scroll_to = true;
}

// set out to line that covers or is closest to y
static void closest_line_y(TextBox box, Style style, int_least32_t y, Line *out) {
    Line lines[2] = {line_create(), line_create()};
    uint_least8_t head = 0;
    first_line(box, style.face, &lines[head]);
    int_least32_t dy = llabs(box.y - y), h = line_height_px(style), baseline_offset = baseline_offset_px(style.face);
    while (true) {
        int_least32_t line_top = lines[head].y - baseline_offset;
        if (y >= line_top && y < line_top + h) break;
        else {
            int_least32_t dy1 = min(labs(line_top - y), labs(line_top + h - y));
            if (dy1 < dy) dy = dy1;
            else break; // following lines only increase distance
        }
        if (!next_line(box, style, lines + head, lines + ((head + 1) % 2))) break;
        head = (head + 1) % 2;
    }
    line_copy(out, &lines[head]);
    line_destroy(lines[0]);
    line_destroy(lines[1]);
}

void cursor_mouse(TextBox box, Style style, Cursor *cursor, int_least32_t x, int_least32_t y, bool selecting) {
    sel_deactivated(cursor, selecting);
    Line line = line_create();
    closest_line_y(box, style, y - box.scroll_y, &line);
    cursor_to_closest_x(box, line, cursor, x);
    cursor->update_sticky_x = true;
    cursor->scroll_to = true;
}

void cursor_page_up(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_first(cursor->loc, cursor->sel_start);
    Line line = line_create();
    shape_line(box, style, cursor->loc.offset, cursor->loc.pre_wrap, &line);
    if (line.offset == 0) {
        if (cursor->loc.offset == 0) cursor->update_sticky_x = true;
        else cursor->loc.offset = 0;
    } else {
        update_sticky_x(box, style, cursor);
        int_least32_t h = line_height_px(style), b = baseline_offset_px(style.face), target_y = line.y + (h - b) - box.h;
        closest_line_y(box, style, target_y, &line);
        if ((target_y < line.y - b || target_y >= line.y + (h - b)) && line.offset == 0) cursor->loc.offset = 0;
        else cursor_to_closest_x(box, line, cursor, cursor->loc.sticky_x);
    }
    line_destroy(line);
    cursor->scroll_to = true;
}

void cursor_page_down(TextBox box, Style style, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_last(cursor->loc, cursor->sel_start);
    Line line = line_create();
    shape_line(box, style, cursor->loc.offset, cursor->loc.pre_wrap, &line);
    if (line.offset + line.count == box.codepoint_count) {
        if (cursor->loc.offset == box.codepoint_count) cursor->update_sticky_x = true;
        else cursor->loc.offset = box.codepoint_count;
    } else {
        update_sticky_x(box, style, cursor);
        int_least32_t b = baseline_offset_px(style.face), target_y = line.y - b + box.h;
        closest_line_y(box, style, target_y, &line);
        if ((target_y < line.y - b || target_y >= line.y + (line_height_px(style) - b)) && line.offset + line.count == box.codepoint_count) {
            cursor->loc.offset = box.codepoint_count;
        } else cursor_to_closest_x(box, line, cursor, cursor->loc.sticky_x);
    }
    line_destroy(line);
    cursor->scroll_to = true;
}

// simplified. Following UAX #29 (https://www.unicode.org/reports/tr29/#Word_Boundaries) needs multiple large bitmaps to check Is_Alphabetic,...
static bool is_word_boundary(uint32_t prev_cp, uint32_t cp) {
    return (prev_cp == LF && cp == LF) || (prev_cp != LF && cp == LF) || (prev_cp != SPACE && cp == SPACE);
}

void cursor_next_word(TextBox box, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_last(cursor->loc, cursor->sel_start);
    if (cursor->loc.offset < box.codepoint_count) {
        uint32_t cp, prev_cp = box.codepoints[cursor->loc.offset];
        while (++cursor->loc.offset < box.codepoint_count) {
            cp = box.codepoints[cursor->loc.offset];
            if (is_word_boundary(prev_cp, cp)) break;
            prev_cp = cp;
        }
    }
    cursor->loc.pre_wrap = true;
    cursor->scroll_to = true;
    cursor->update_sticky_x = true;
}

void cursor_prev_word(TextBox box, Cursor *cursor, bool selecting) {
    if (sel_deactivated(cursor, selecting)) cursor->loc = loc_first(cursor->loc, cursor->sel_start);
    if (cursor->loc.offset > 0) {
        uint32_t cp, prev_cp = box.codepoints[cursor->loc.offset - 1];
        while (--cursor->loc.offset > 0) {
            cp = box.codepoints[cursor->loc.offset - 1];
            if (is_word_boundary(prev_cp, cp)) break;
            prev_cp = cp;
        }
    }
    cursor->loc.pre_wrap = false;
    cursor->scroll_to = true;
    cursor->update_sticky_x = true;
}