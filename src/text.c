#include "bane.h"
#include "firasans_regular.c"

// LINE

typedef enum LineEnd {
    END_WRAP = 0, // Cursor at line end may jump to next line if pre_wrap is false
    END_LF = 1,   // line.xs holds one less value and cursor after \n should display in next line
    END_INPUT = 2 // Cursor at line end does not jump to next line regardless of pre_wrap
} LineEnd;

sb_declare(int_least32_t);

typedef struct Line {
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
            (offset == end_offset && (line.end == END_INPUT || (line.end == END_WRAP && pre_wrap)))
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

// GAP BUFFER

GapBuffer gb_create(size_t cap) {
    GapBuffer gb = {.cap = cap, .gap = cap, .offset = 0, .data = malloc(sizeof(uint32_t) * cap)};
    ensure(gb.data != NULL);
    return gb;
}

GapBuffer gb_create_from_text(const char *text, size_t str_len) {
    assert(text != NULL && str_len > 0);
    size_t len;
    ensure(!utf8_measure_codepoints(text, str_len, &len, NULL) && len * 2 > len);
    GapBuffer gb = gb_create(len * 2);
    ensure(!utf8_decode(text, str_len, true, len, gb.data, NULL));
    gb.offset = len;
    gb.gap = len;
    return gb;
}

void gb_destroy(GapBuffer *gb) { free(gb->data); memset(gb, 0, sizeof(GapBuffer)); }

char *gb_encode(GapBuffer gb, size_t offset, size_t length) {
    assert(offset + length <= gb_count(gb));
    size_t pre_gap = offset < gb.offset ? min(gb.offset - offset, length) : 0, pre_gap_bytes;
    ensure(!utf8_measure_bytes(gb.data + offset, pre_gap, true, &pre_gap_bytes));
    size_t post_gap = length - pre_gap, post_gap_bytes;
    ensure(!utf8_measure_bytes(gb.data + gb.gap + max(gb.offset, offset), post_gap, true, &post_gap_bytes));
    size_t to_write = pre_gap_bytes + post_gap_bytes + 1, written = 0; // +1 for '\0'
    char *ret = malloc(sizeof(char) * to_write); 
    if (pre_gap) ensure(!utf8_encode(gb.data + offset, pre_gap, true, to_write, ret, &written));
    if (post_gap) ensure(!utf8_encode(gb.data + gb.gap + max(gb.offset, offset), post_gap, true, to_write - written, ret + written, NULL));
    return ret;
}

static void gb_move_gap(GapBuffer *gb, size_t index) {
    assert(gb != NULL && index <= gb_count(*gb));
    if (gb->gap > 0) {
        if (index < gb->offset) memmove(gb->data + index + gb->gap, gb->data + index, sizeof(uint32_t) * (gb->offset - index));
        else if (index > gb->offset) memmove(gb->data + gb->offset, gb->data + gb->gap + gb->offset, sizeof(uint32_t) * (index - gb->offset));
    }
    gb->offset = index;
}

size_t gb_count(GapBuffer gb) { return gb.cap - gb.gap; }

void gb_insert(GapBuffer *gb, size_t index, uint32_t value) { gb_insert_n(gb, index, &value, 1); }

void gb_insert_n(GapBuffer *gb, size_t index, uint32_t *values, size_t n) {
    if (n == 0) return;
    assert(values != NULL);
    assert(index <= gb_count(*gb));
    if (gb->gap < n) {
        gb_move_gap(gb, gb_count(*gb)); // ensure gap is contiguous after realloc
        size_t new_cap = max(max(16u, gb->cap * 2), gb->cap + n);
        ensure(new_cap > gb->cap);
        gb->gap = new_cap - gb->cap + gb->gap;
        gb->cap = new_cap;
        gb->data = realloc(gb->data, sizeof(uint32_t) * gb->cap);
        ensure(gb->data != NULL);
    }
    gb_move_gap(gb, index);
    memmove(gb->data + gb->offset, values, sizeof(uint32_t) * n);
    gb->offset = index + n;
    gb->gap -= n;
}

void gb_delete_n(GapBuffer *gb, size_t index, size_t n) {
    if (n == 0) return;
    assert(index + n <= gb_count(*gb));
    if (index != gb->offset) {
        gb_move_gap(gb, index + n);
        gb->offset -= n;
    }
    gb->gap += n;
}

uint32_t gb_get(GapBuffer gb, size_t index) {
    assert(index < gb_count(gb));
    return gb.data[index >= gb.offset ? index + gb.gap : index];
}

// SELECTION

static bool sel_collapses(Cursor c) { return c.loc.offset == c.sel_origin.offset && c.loc.pre_wrap == c.sel_origin.pre_wrap; }
static bool locs_reversed(CursorLoc l1, CursorLoc l2) { return l1.offset > l2.offset || (l1.offset == l2.offset && l2.pre_wrap); }
static CursorLoc sel_start(Cursor c) { return locs_reversed(c.loc, c.sel_origin) ? c.sel_origin : c.loc; }
static CursorLoc sel_end(Cursor c) { return locs_reversed(c.loc, c.sel_origin) ? c.loc : c.sel_origin; }
// update selection and return whether it changed from active to inactive
static bool sel_deactivated(Cursor *cursor, bool selecting) {
    if (cursor->sel_active) cursor->sel_active = !sel_collapses(*cursor);
    if (selecting) {
        if (!cursor->sel_active) cursor->sel_origin = cursor->loc;
        cursor->sel_active = true;
        return false;
    } else {
        bool deactivated = cursor->sel_active;
        cursor->sel_active = false;
        return deactivated;
    }
}

// RENDER CONTEXT

void renderctx_create(RenderContext *ctx) {
    // 960x540 helps reduce wasted memory at common resolutions if doubled upon resize
    ctx->fb = (FrameBuffer) {.max_width = 960, .max_height = 540, .image.format = IMAGE_FORMAT_RGB};
    ctx->fb.data = malloc(ctx->fb.max_width * ctx->fb.max_height * 3);
    ensure(ctx->fb.data != NULL);
    // image and txture created upon resize, when real actual is known

    ensure(!FT_Init_FreeType(&ctx->library));
    ensure(!FT_New_Memory_Face(ctx->library, (FT_Byte *) assets_FiraSans_Regular_ttf, assets_FiraSans_Regular_ttf_len, 0, &ctx->face));
    ctx->atlas = texture_atlas_create(4096);
}

void renderctx_load_style(RenderContext *ctx, Style style) {
    ensure(!FT_Set_Char_Size(ctx->face, 0, style.font_size*64*SCALE, 72, 72 ));
    // NOTE different to CSS behavior, where line-height is multiplier of font-size.
    // Here it is multiplier of computed baseline-to-baseline distance.
    // Deep dive CSS: font metrics, line-height and vertical-align: https://iamvdo.me/en/blog/css-font-metrics-line-height-and-vertical-align
    ctx->line_height = style.line_height * ((ctx->face->size->metrics.height + 32) >> 6);
    // property not directly accessible through FT_Face
    int_least32_t line_gap = ctx->face->size->metrics.height - (ctx->face->size->metrics.ascender - ctx->face->size->metrics.descender);
    ctx->baseline_offset = (ctx->face->size->metrics.ascender + (line_gap / 2) + 32) >> 6;
    ctx->text_color = style.text_color;
    ctx->text_selected_color = style.text_selected_color;
    ctx->cursor_color = style.cursor_color;
    ctx->selection_color = style.selection_color;
}

void renderctx_destroy(RenderContext *ctx) {
    texture_atlas_destroy(&ctx->atlas);
    FT_Done_Face(ctx->face);
    FT_Done_FreeType(ctx->library);
    memset(ctx, 0, sizeof(RenderContext));
}

// TEXTBOX

TextBox textbox_create(int_least32_t x, int_least32_t y, int_least32_t w, int_least32_t h, const char* text) {
    return (TextBox) {.x = x, .y = y, .w = w, .h = h, .gb = gb_create_from_text(text, strlen(text)), .scroll_y = 0};
}

bool tb_hit(TextBox box, int_least32_t x, int_least32_t y) { return x >= box.x && x < box.x + box.w && y >= box.y && y < box.y + box.h; }

// Shapes exactly one line worth of text. Only populates out's .xs, .count .end
static void shape_text(GapBuffer gb, size_t offset, FT_Face face, int_least32_t w, Line *out) {
    assert(out != NULL);
    out->xs.count = 0;
    sb_append(&out->xs, 0);
    out->end = END_INPUT;
    uint_least64_t pen_x = 0; // 64ths of a pixel to prevent accumulating error from adding rounded advance widths
    size_t max_len = gb_count(gb) - offset, i, last_space = 0; // last_space counts characters up to and including the most recent space
    for (i = 0; i < max_len; i++) {
        uint32_t cp = gb_get(gb, offset + i);
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

static void first_line(TextBox box, RenderContext *ctx, Line *out) {
    assert(ctx != NULL && out != NULL);
    out->y = box.y + ctx->baseline_offset;
    out->offset = 0;
    shape_text(box.gb, 0, ctx->face, box.w, out);
}

// populates out with line after in. Returns whether there is a line after in. If there isn't out is not modified!
// NOTE: next_line is strongly assumed to successfully reach all valid positions in predictable system state.
// So a pattern like "if (!next_line) abort()" is deemed acceptable
static bool next_line(TextBox box, RenderContext *ctx, Line *in, Line *out) {
    assert(ctx != NULL && in != NULL && out != NULL);
    size_t new_offset = in->offset + in->count;
    if (new_offset > gb_count(box.gb) || (new_offset == gb_count(box.gb) && in->end != END_LF)) return false; // no line after in
    out->y = in->y + ctx->line_height;
    out->offset = new_offset;
    shape_text(box.gb, out->offset, ctx->face, box.w, out);
    return true;
}

// Populates out with all attributes of Line covering (offset, pre_wrap) in the text
static void shape_line(TextBox box, RenderContext *ctx, size_t offset, bool pre_wrap, Line *out) {
    assert(ctx != NULL && out != NULL);
    assert(offset <= gb_count(box.gb));
    first_line(box, ctx, out);
    while (!line_contains(*out, offset, pre_wrap)) ensure(next_line(box, ctx, out, out));
}

// return entry from line.xs corresponding to offset. NOTE: Returned value is still *relative* to line left edge.
// To ensure line has corresponding x, use line_contains or use lines from shape_line
static int_least32_t x_offset(Line line, size_t offset) {
    if (line.offset == offset) return 0;
    assert(offset > line.offset);
    size_t idx = offset - line.offset;
    assert(line.xs.count > idx);
    return sb_get(line.xs, idx);
}

static void draw_char(FrameBuffer fb, Rectangle box, FT_Face face, TextureAtlas *atlas, uint32_t cp, int_least32_t x, int_least32_t y, Color color) {
    assert(atlas != NULL);
    if (cp == LF || cp == SPACE) return;
    FT_UInt glyph_index = FT_Get_Char_Index(face, cp);
    TextureRect rect;
    TaError ta_error = texture_atlas_get_rect(*atlas, glyph_index, &rect);
    if (ta_error == TA_RECT_NOT_FOUND) {
        FT_GlyphSlot slot = face->glyph;
        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        assert(!error);
        assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported
        Image image = image_create_from_data(slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, IMAGE_FORMAT_ALPHA);
        ta_error = texture_atlas_add_get_rect(atlas, glyph_index, image, slot->bitmap_left, slot->bitmap_top, &rect);
        assert(!ta_error);
    }
    ta_error = texture_atlas_draw(fb.image, box, atlas, glyph_index, x, y, color);
    assert(!ta_error);
}

static void sel_draw_rect(FrameBuffer fb, Rectangle text_box, Rectangle rect, Color color) {
    rect_fit_box(text_box, &rect, NULL);
    image_draw_rect(fb.image, rect, color);
}

void tb_draw(TextBox *box, RenderContext *ctx) {
    // characters aren't drawn one by one because it takes preprocessing the whole line to know where it wraps
    Line line = line_create();
    Cursor *c = &box->cursor;
    int_least32_t cx, cy;
    Rectangle bounding_box = {box->x, box->y, box->w, box->h};
    shape_line(*box, ctx, c->loc.offset, c->loc.pre_wrap, &line);
    cx = box->x + x_offset(line, c->loc.offset);
    cy = line.y - ctx->baseline_offset;
    if (c->scroll_to) { // scroll into view
        if (cy + box->scroll_y < box->y) box->scroll_y = box->y - cy;
        else if (cy + ctx->line_height + box->scroll_y > box->y + box->h) box->scroll_y = box->y + box->h - (cy + ctx->line_height);
        c->scroll_to = false;
    }
    first_line(*box, ctx, &line);
    // skip invisible lines
    while (line.y - ctx->baseline_offset + ctx->line_height + box->scroll_y <= box->y) if (!next_line(*box, ctx, &line, &line)) return; 
    if (c->sel_active && !sel_collapses(*c)) {
        // draw text + selection together
        CursorLoc start, end;
        if (locs_reversed(c->loc, c->sel_origin)) { start = c->sel_origin; end = c->loc;}
        else { start = c->loc; end = c->sel_origin; }
        do {
            // draw selection
            if (line_contains(line, start.offset, start.pre_wrap)) {
                int_least32_t x = box->x + x_offset(line, start.offset), y = line.y - ctx->baseline_offset + box->scroll_y, w;
                if (line_contains(line, end.offset, end.pre_wrap)) w = box->x + x_offset(line, end.offset) - x;
                else w = box->x + sb_rget(line.xs, 1) - x + (line.end == END_LF ? 4 : 0);
                sel_draw_rect(ctx->fb, bounding_box, (Rectangle) {x, y, w, ctx->line_height}, ctx->selection_color);
            } else if (line.offset + line.count > start.offset && line.offset <= end.offset) {
                int_least32_t x = box->x, y = line.y - ctx->baseline_offset + box->scroll_y, w;
                if (line_contains(line, end.offset, end.pre_wrap)) w = box->x + x_offset(line, end.offset) - x;
                else w = sb_rget(line.xs, 1) + (line.end == END_LF ? 4 : 0);
                sel_draw_rect(ctx->fb, bounding_box, (Rectangle) {x, y, w, ctx->line_height}, ctx->selection_color);
            }
            // draw text
            int_least32_t y = line.y + box->scroll_y;
            bool in_selection = false;
            for (size_t i = 0; i < line.count; i++) {
                bool before_end = line.offset + i < end.offset;
                in_selection = in_selection ? before_end : line.offset + i >= start.offset && before_end;
                Color color = in_selection ? ctx->text_selected_color : ctx->text_color;
                int_least32_t x = box->x + sb_get(line.xs, i);
                draw_char(ctx->fb, bounding_box, ctx->face, &ctx->atlas, gb_get(box->gb, line.offset + i), x, y, color);
            }
        } while (line.y + box->scroll_y < box->y + box->h && next_line(*box, ctx, &line, &line));
    } else do {
        // draw only text
        int_least32_t y = line.y + box->scroll_y;
        for (size_t i = 0; i < line.count; i++) {
            int_least32_t x = box->x + sb_get(line.xs, i);
            draw_char(ctx->fb, bounding_box, ctx->face, &ctx->atlas, gb_get(box->gb, line.offset + i), x, y, ctx->text_color);
        }
    } while (line.y + box->scroll_y < box->y + box->h && next_line(*box, ctx, &line, &line));
    line_destroy(line);
    // draw cursor. Drawn at end to layer over text and selection
    if (cy + box->scroll_y >= box->y && cy + box->scroll_y < box->y + box->h) {
        image_draw_rect(ctx->fb.image, (Rectangle) {cx, cy + box->scroll_y, 1, ctx->line_height}, ctx->cursor_color);
    }
}

void tb_right(TextBox *box, bool selecting) { tb_right_n(box, 1, selecting); }

void tb_right_n(TextBox *box, size_t n, bool selecting) {
    if (n > 0) {
        Cursor *c = &box->cursor;
        bool deselected = sel_deactivated(c, selecting);
        if (deselected) c->loc = sel_end(*c);
        if (!deselected || --n) {
            c->loc.offset = min(gb_count(box->gb), c->loc.offset + n);
            c->loc.pre_wrap = true;
            c->update_sticky_x = true;
        }
        c->scroll_to = true;
    }
}

void tb_left(TextBox *box, bool selecting) {
    Cursor *c = &box->cursor;
    if (sel_deactivated(c, selecting)) c->loc = sel_start(*c);
    else {
        if (c->loc.offset > 0) c->loc.offset--;
        c->loc.pre_wrap = false;
        c->update_sticky_x = true;
    }
    c->scroll_to = true;
}

// set cursor offset and prewrap to what is closest to absolute x
static void cursor_to_closest_x(TextBox *box, Line line, int_least32_t x) {
    assert(box != NULL);
    Cursor *c = &box->cursor;
    c->loc.offset = line.offset;
    c->loc.pre_wrap = false;
    uint_least32_t d = labs(box->x - x);
    for (size_t i = 1; i < line.xs.count; i++) { // could be binary search for speed. starts at i == 1 because sb_get(line.xs, 0) = 0
        uint_least32_t d0 = labs(box->x + sb_get(line.xs, i) - x);
        if (d0 < d) {
            d = d0;
            c->loc.offset = line.offset + i;
            c->loc.pre_wrap = true;
        }
    }
}

// to call by functions that use cursor->loc.sticky_x before using it. requires shaping lines, so only used when necessary
static void update_sticky_x(TextBox *box, RenderContext *ctx) {
    assert(box != NULL && ctx != NULL);
    Cursor *c = &box->cursor;
    if (c->update_sticky_x) {
        Line line = line_create();
        shape_line(*box, ctx, c->loc.offset, c->loc.pre_wrap, &line);
        c->loc.sticky_x = box->x + x_offset(line, c->loc.offset);
        c->update_sticky_x = false;
        line_destroy(line);
    }
}

void tb_down(TextBox *box, RenderContext *ctx, bool selecting) {
    Cursor *c = &box->cursor;
    if (sel_deactivated(c, selecting)) c->loc = sel_end(*c);
    if (c->loc.offset < gb_count(box->gb)) {
        update_sticky_x(box, ctx);
        Line line = line_create();
        shape_line(*box, ctx, c->loc.offset, c->loc.pre_wrap, &line);
        if (!next_line(*box, ctx, &line, &line)) c->loc.offset = line.offset + line.count;
        else cursor_to_closest_x(box, line, c->loc.sticky_x);
        line_destroy(line);
    } else c->update_sticky_x = true;
    c->scroll_to = true;
}

void tb_up(TextBox *box, RenderContext *ctx, bool selecting) {
    Cursor *c = &box->cursor;
    if (sel_deactivated(c, selecting)) c->loc = sel_start(*c);
    if (c->loc.offset > 0) {
        update_sticky_x(box, ctx);
        Line lines[2] = {line_create(), line_create()};
        uint_least8_t head = 0;
        first_line(*box, ctx, &lines[head]);
        while(!line_contains(lines[head], c->loc.offset, c->loc.pre_wrap)) {
            ensure(next_line(*box, ctx, lines + head, lines + ((head + 1) % 2)));
            head = (head + 1) % 2;
        }
        if (lines[head].offset == 0) { // no previous line
            c->loc.pre_wrap = false;
            c->loc.offset = 0;
        } else cursor_to_closest_x(box, lines[labs(head - 1) % 2], c->loc.sticky_x);
        line_destroy(lines[0]);
        line_destroy(lines[1]);
    } else c->update_sticky_x = true;
    c->scroll_to = true;
}

void tb_home(TextBox *box, RenderContext *ctx, bool selecting) {
    Cursor *c = &box->cursor;
    if (sel_deactivated(c, selecting)) c->loc = sel_start(*c);
    if (c->loc.offset > 0) {
        Line line = line_create();
        shape_line(*box, ctx, c->loc.offset, c->loc.pre_wrap, &line);
        if (c->loc.offset == line.offset) {
            for (; c->loc.offset > 0; c->loc.offset--) if (gb_get(box->gb, c->loc.offset - 1) == LF) break;
        } else c->loc.offset = line.offset;
        line_destroy(line);
    }
    c->loc.pre_wrap = false;
    c->update_sticky_x = true;
    c->scroll_to = true;
}

void tb_end(TextBox *box, RenderContext *ctx, bool selecting) {
    Cursor *c = &box->cursor;
    if (sel_deactivated(c, selecting)) c->loc = sel_end(*c);
    if (c->loc.offset < gb_count(box->gb)) {
        Line line = line_create();
        shape_line(*box, ctx, c->loc.offset, c->loc.pre_wrap, &line);
        size_t last_offset = line.offset + line.xs.count - 1;
        if (c->loc.offset == last_offset) {
            for (; c->loc.offset < gb_count(box->gb); c->loc.offset++) if (gb_get(box->gb, c->loc.offset) == LF) break;
        } else c->loc.offset = last_offset;
        line_destroy(line);
    }
    c->loc.pre_wrap = true;
    c->update_sticky_x = true;
    c->scroll_to = true;
}

// set `out` to line that covers or is closest to y
static void closest_line_y(TextBox box, RenderContext *ctx, int_least32_t y, Line *out) {
    assert(ctx != NULL && out != NULL);
    Line lines[2] = {line_create(), line_create()};
    uint_least8_t head = 0;
    first_line(box, ctx, &lines[head]);
    int_least32_t dy = llabs(box.y - y);
    while (true) {
        int_least32_t line_top = lines[head].y - ctx->baseline_offset;
        if (y >= line_top && y < line_top + ctx->line_height) break;
        else {
            int_least32_t dy1 = min(labs(line_top - y), labs(line_top + ctx->line_height - y));
            if (dy1 < dy) dy = dy1;
            else break; // following lines only increase distance
        }
        if (!next_line(box, ctx, lines + head, lines + ((head + 1) % 2))) break;
        head = (head + 1) % 2;
    }
    line_copy(out, &lines[head]);
    line_destroy(lines[0]);
    line_destroy(lines[1]);
}

void tb_mouse(TextBox *box, RenderContext *ctx, int_least32_t x, int_least32_t y, bool selecting) {
    sel_deactivated(&box->cursor, selecting);
    Line line = line_create();
    closest_line_y(*box, ctx, y - box->scroll_y, &line);
    cursor_to_closest_x(box, line, x);
    box->cursor.update_sticky_x = true;
    box->cursor.scroll_to = true;
    line_destroy(line);
}

void tb_page_up(TextBox *box, RenderContext *ctx, bool selecting) {
    Cursor *c = &box->cursor;
    if (sel_deactivated(c, selecting)) c->loc = sel_start(*c);
    Line line = line_create();
    shape_line(*box, ctx, c->loc.offset, c->loc.pre_wrap, &line);
    if (line.offset == 0) {
        if (c->loc.offset == 0) c->update_sticky_x = true;
        else c->loc.offset = 0;
    } else {
        update_sticky_x(box, ctx);
        int_least32_t target_y = line.y + (ctx->line_height - ctx->baseline_offset) - box->h;
        closest_line_y(*box, ctx, target_y, &line);
        if ((target_y < line.y - ctx->baseline_offset || target_y >= line.y + (ctx->line_height - ctx->baseline_offset)) && line.offset == 0) {
            c->loc.offset = 0;
        }
        else cursor_to_closest_x(box, line, c->loc.sticky_x);
    }
    line_destroy(line);
    c->scroll_to = true;
}

void tb_page_down(TextBox *box, RenderContext *ctx, bool selecting) {
    Cursor *c = &box->cursor;
    if (sel_deactivated(c, selecting)) c->loc = sel_end(*c);
    Line line = line_create();
    shape_line(*box, ctx, c->loc.offset, c->loc.pre_wrap, &line);
    if (line.offset + line.count == gb_count(box->gb)) {
        if (c->loc.offset == gb_count(box->gb)) c->update_sticky_x = true;
        else c->loc.offset = gb_count(box->gb);
    } else {
        update_sticky_x(box, ctx);
        int_least32_t target_y = line.y - ctx->baseline_offset + box->h;
        closest_line_y(*box, ctx, target_y, &line);
        if ((target_y < line.y - ctx->baseline_offset || target_y >= line.y + (ctx->line_height - ctx->baseline_offset)) &&
        line.offset + line.count == gb_count(box->gb)) {
            c->loc.offset = gb_count(box->gb);
        } else cursor_to_closest_x(box, line, c->loc.sticky_x);
    }
    line_destroy(line);
    c->scroll_to = true;
}

// simplified. Following UAX #29 (https://www.unicode.org/reports/tr29/#Word_Boundaries) needs multiple large bitmaps to check Is_Alphabetic,...
static bool is_word_boundary(uint32_t prev_cp, uint32_t cp) {
    return (prev_cp == LF && cp == LF) || (prev_cp != LF && cp == LF) || (prev_cp != SPACE && cp == SPACE);
}

void tb_next_word(TextBox *box, bool selecting) {
    Cursor *c = &box->cursor;
    if (sel_deactivated(c, selecting)) c->loc = sel_end(*c);
    if (c->loc.offset < gb_count(box->gb)) {
        uint32_t cp, prev_cp = gb_get(box->gb, c->loc.offset);
        while (++c->loc.offset < gb_count(box->gb)) {
            cp = gb_get(box->gb, c->loc.offset);
            if (is_word_boundary(prev_cp, cp)) break;
            prev_cp = cp;
        }
    }
    c->loc.pre_wrap = true;
    c->scroll_to = true;
    c->update_sticky_x = true;
}

void tb_prev_word(TextBox *box, bool selecting) {
    Cursor *c = &box->cursor;
    if (sel_deactivated(c, selecting)) c->loc = sel_start(*c);
    if (c->loc.offset > 0) {
        uint32_t cp, prev_cp = gb_get(box->gb, c->loc.offset - 1);
        while (--c->loc.offset > 0) {
            cp = gb_get(box->gb, c->loc.offset - 1);
            if (is_word_boundary(prev_cp, cp)) break;
            prev_cp = cp;
        }
    }
    c->loc.pre_wrap = false;
    c->scroll_to = true;
    c->update_sticky_x = true;
}

static void sel_delete(GapBuffer *gb, Cursor *cursor) {
    assert(gb != NULL && cursor != NULL);
    CursorLoc start = sel_start(*cursor);
    gb_delete_n(gb, start.offset, sel_end(*cursor).offset - start.offset);
    cursor->loc = start;
    cursor->sel_active = false;
}

void tb_write(TextBox *box, uint32_t c) { tb_write_n(box, &c, 1); }

void tb_write_n(TextBox *box, uint32_t *c, size_t n) {
    if (box->cursor.sel_active) sel_delete(&box->gb, &box->cursor);
    gb_insert_n(&box->gb, box->cursor.loc.offset, c, n);
    tb_right_n(box, n, false);
}

void tb_backspace(TextBox *box) {
    Cursor *c = &box->cursor;
    if (c->sel_active) sel_delete(&box->gb, c);
    else if (c->loc.offset > 0) {
        gb_delete_n(&box->gb, c->loc.offset - 1, 1);
        c->loc.offset--;
        c->update_sticky_x = true;
        c->loc.pre_wrap = false;
    }
    c->scroll_to = true;
}

void tb_delete(TextBox *box) {
    Cursor *c = &box->cursor;
    if (c->sel_active) sel_delete(&box->gb, c);
    else if (c->loc.offset < gb_count(box->gb)) gb_delete_n(&box->gb, c->loc.offset, 1);
    c->scroll_to = true;
}

void tb_copy(TextBox box) {
    if (box.cursor.sel_active) {
        size_t offset = sel_start(box.cursor).offset, length = sel_end(box.cursor).offset - offset;
        char *text = gb_encode(box.gb, offset, length);
        if (!SDL_SetClipboardText(text)) SDL_Log("Unable to set clipboard string: %s", SDL_GetError());
        free(text);
    }
}

void tb_paste(TextBox *box) {
    Cursor *c = &box->cursor;
    if (c->sel_active) sel_delete(&box->gb, c);
    char *text = SDL_GetClipboardText();
    if (text) {
        size_t len, str_len = strlen(text);
        Utf8Error error = utf8_measure_codepoints(text, str_len, &len, NULL);
        ensure(!error);
        uint32_t *codepoints = malloc(sizeof(uint32_t) * len);
        error = utf8_decode(text, str_len, true, len, codepoints, NULL);
        ensure(!error);
        gb_insert_n(&box->gb, c->loc.offset, codepoints, len);
        free(codepoints);
        c->loc.offset += len;
        c->scroll_to = true;
        c->loc.pre_wrap = true;
        c->update_sticky_x = true;
    }
    SDL_free(text);
}

void tb_cut(TextBox *box) {
    tb_copy(*box);
    if (box->cursor.sel_active) sel_delete(&box->gb, &box->cursor);
}

void tb_select_all(TextBox *box) {
    Cursor *c = &box->cursor;
    c->sel_origin.offset = 0;
    c->sel_origin.pre_wrap = true;
    c->sel_origin.sticky_x = box->x;
    c->loc.offset = gb_count(box->gb);
    c->loc.pre_wrap = false;
    c->update_sticky_x = true;
    c->sel_active = true;
}