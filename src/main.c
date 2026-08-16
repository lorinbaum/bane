#include <stdio.h>
#include <math.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "bane.h"

#include <SDL3/SDL_main.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
Image framebuffer = { 0 };
size_t framebuffer_max_width = 0, framebuffer_max_height = 0;
#define WINDOW_WIDTH_BASE 960U
#define WINDOW_HEIGHT_BASE 540U
bool draw = false;

char *text = u8"assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported\n"
    "Image texture = {(void *) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};\n"
    "status = texture_atlas_add_get_rect(&rect, atlas, glyph_index, texture, slot->bitmap_left, slot->bitmap_top);\n"
    "assert(status == TA_OK);\n\n"
    "assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported\na";

TextBox *last_hit = NULL;
Context *context = NULL;
TextBox textbox;

Style style = {
    .font_size = 20,
    .cursor_color = {0xff, 0xff, 0xff, 0xff},
    .line_height = 1,
    .selection_color = {0xe3, 0x88, 0x64, 0xff},
    .text_color = {0xe3, 0x88, 0x64, 0xff},
    .text_selected_color = {0xff, 0xff, 0xff, 0xff}
};

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    /* Create the window */
    unsigned long flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (!SDL_CreateWindowAndRenderer("Hello World", WINDOW_WIDTH_BASE, WINDOW_HEIGHT_BASE, flags, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    framebuffer.format = IMAGE_FORMAT_RGB;

    context = ctx_create();
    ctx_load_style(context, style);
    textbox = textbox_create(500, 500, 100, 80, text);

    if (!SDL_StartTextInput(window)) {
        SDL_Log("Couldn't start text input: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

static bool is_selecting(SDL_Keymod mod) { return mod & SDL_KMOD_SHIFT && !(mod & (SDL_KMOD_ALT | SDL_KMOD_CTRL | SDL_KMOD_MODE | SDL_KMOD_GUI)); }

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN: {
            bool shift = is_selecting(event->key.mod);
            bool ctrl = event->key.mod & SDL_KMOD_CTRL && !(event->key.mod & (SDL_KMOD_ALT | SDL_KMOD_SHIFT | SDL_KMOD_MODE | SDL_KMOD_GUI));
            bool ctrlshift = (event->key.mod & SDL_KMOD_CTRL) && event->key.mod & SDL_KMOD_SHIFT && !(event->key.mod & (SDL_KMOD_ALT | SDL_KMOD_MODE | SDL_KMOD_GUI));
            switch (event->key.key) {
                case SDLK_ESCAPE: return SDL_APP_SUCCESS;
                case SDLK_RIGHT:
                    if (ctrl || ctrlshift) tb_next_word(&textbox, ctrlshift);
                    else tb_right(&textbox, shift);
                    break;
                case SDLK_LEFT:
                    if (ctrl || ctrlshift) tb_prev_word(&textbox, ctrlshift);
                    else tb_left(&textbox, shift);
                    break;
                case SDLK_DOWN: tb_down(&textbox, context, shift); break;
                case SDLK_UP: tb_up(&textbox, context, shift); break;
                case SDLK_HOME: tb_home(&textbox, context, shift); break;
                case SDLK_END: tb_end(&textbox, context, shift); break;
                case SDLK_PAGEUP: tb_page_up(&textbox, context, shift); break;
                case SDLK_PAGEDOWN: tb_page_down(&textbox, context, shift); break;
                case SDLK_C: if (ctrl) tb_copy(textbox); break;
                case SDLK_V: if (ctrl) tb_paste(&textbox); break;
                case SDLK_X: if (ctrl) tb_cut(&textbox); break;
                case SDLK_A: if (ctrl) tb_select_all(&textbox); break;
                case SDLK_BACKSPACE: tb_backspace(&textbox); break;
                case SDLK_DELETE: tb_delete(&textbox); break;
                case SDLK_KP_ENTER: case SDLK_RETURN: tb_write(&textbox, LF); break;
            }
            draw = true;
            break;
        }
        case SDL_EVENT_TEXT_INPUT: {
            size_t len, text_len = strlen(event->text.text);
            ensure(!utf8_measure_codepoints(event->text.text, text_len, &len, NULL));
            uint32_t *codepoints = malloc(len * sizeof(uint32_t));
            ensure(codepoints != NULL);
            ensure(!utf8_decode(event->text.text, text_len, true, len, codepoints, NULL));
            tb_write_n(&textbox, codepoints, len);
            free(codepoints);
            draw = true;
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (event->button.button == SDL_BUTTON_LEFT) {
                int_least32_t x = (int_least32_t) (event->button.x * SCALE + 0.5), y = (int_least32_t) (event->button.y * SCALE + 0.5);
                if (tb_hit(textbox, x, y)) {
                    last_hit = &textbox;
                    tb_mouse(&textbox, context, x, y, is_selecting(event->key.mod));
                    draw = true;
                } else last_hit = NULL;
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (event->motion.state & SDL_BUTTON_LMASK) {
                int_least32_t x = (int_least32_t) (event->motion.x * SCALE + 0.5), y = (int_least32_t) (event->motion.y * SCALE + 0.5);
                if (last_hit == &textbox) { tb_mouse(&textbox, context, x, y, true); draw = true; }
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            int_least32_t x = (int_least32_t) (event->wheel.mouse_x * SCALE + 0.5), y = (int_least32_t) (event->wheel.mouse_y * SCALE + 0.5);
            if (tb_hit(textbox, x, y)) {
                int_least32_t scroll = event->wheel.integer_x * framebuffer.height / 10;
                textbox.scroll_y = min(0, textbox.scroll_y + (event->wheel.direction == SDL_MOUSEWHEEL_NORMAL ? scroll : -scroll));
                draw = true;
            }
            break;
        }
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            // NOTE: event -> window.data don't hold updated dimensions but what was entered when creating the window
            uint w, h;
            if (!SDL_GetRenderOutputSize(renderer, (int *) &w, (int *) &h)) {
                SDL_Log("Couldn't GetRenderOutputSize: %s", SDL_GetError());
                return SDL_APP_FAILURE;
            }

            bool resize = false;
            if (w > framebuffer_max_width) {
                if (w < WINDOW_WIDTH_BASE) framebuffer_max_width = WINDOW_WIDTH_BASE;
                else framebuffer_max_width = ((size_t) 1 << (size_t) ceilf(log2f((float) w / WINDOW_WIDTH_BASE))) * WINDOW_WIDTH_BASE;
                resize = true;
            }
            if (h > framebuffer_max_height) {
                if (w < WINDOW_HEIGHT_BASE) framebuffer_max_height = WINDOW_HEIGHT_BASE;
                else framebuffer_max_height = ((size_t) 1 << (size_t) ceilf(log2f((float) h / WINDOW_HEIGHT_BASE))) * WINDOW_HEIGHT_BASE;
                resize = true;
            }
            if (resize) {
                framebuffer.data = realloc(framebuffer.data, framebuffer_max_width * framebuffer_max_height * 3);
                ensure(framebuffer.data != NULL);
                if (texture != NULL) SDL_DestroyTexture(texture);
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, framebuffer_max_width, framebuffer_max_height);
                if (texture == NULL) {
                    SDL_Log("Couldn't CreateTexture: %s", SDL_GetError());
                    return SDL_APP_FAILURE;
                }
            }

            framebuffer.width = w;
            framebuffer.height = h;

            textbox.w = w - 100;
            // textbox.h = h - 100;

            draw = true;
            break;
        }
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    if (draw) {
        image_draw_rect(framebuffer, (Rectangle) {0, 0, framebuffer.width, framebuffer.height}, (Color) {0, 0, 0, 255});

        image_draw_rect(framebuffer, (Rectangle) {textbox.x - 1,         textbox.y - 1,             textbox.w + 1, 1},             (Color) {255, 255, 255, 255});
        image_draw_rect(framebuffer, (Rectangle) {textbox.x - 1,         textbox.y - 1,             1,             textbox.h + 2}, (Color) {255, 255, 255, 255});
        image_draw_rect(framebuffer, (Rectangle) {textbox.x - 1,         textbox.y + textbox.h + 1, textbox.w + 1, 1},             (Color) {255, 255, 255, 255});
        image_draw_rect(framebuffer, (Rectangle) {textbox.x + textbox.w, textbox.y - 1,             1,             textbox.h + 2}, (Color) {255, 255, 255, 255});
    
        tb_draw(&textbox, context);

        SDL_Rect framebuffer_rect = {.x = 0, .y = 0, .w = framebuffer.width, .h = framebuffer.height};
        SDL_UpdateTexture(texture, &framebuffer_rect, framebuffer.data, framebuffer.width * 3);
        SDL_FRect framebuffer_frect = {.x = 0, .y = 0, .w = (float) framebuffer.width, .h = (float) framebuffer.height};
        SDL_RenderTexture(renderer, texture, &framebuffer_frect, NULL);
        SDL_RenderPresent(renderer);
        draw = false;
    }

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_StopTextInput(window);
    gb_destroy(&textbox.gb);
    ctx_destroy(context);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}
