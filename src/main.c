#include <stdio.h>
#include <math.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "bane.h"

#include <SDL3/SDL_main.h>

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    State *state = calloc(1, sizeof(State));
    ensure(state != NULL);
    *appstate = state;

    state->last_hit = NULL;
    char *text = u8"assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported\n"
        "Image texture = {(void *) slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};\n"
        "status = texture_atlas_add_get_rect(&rect, atlas, glyph_index, texture, slot->bitmap_left, slot->bitmap_top);\n"
        "assert(status == TA_OK);\n\n"
        "assert(slot->bitmap.pitch == (int) slot->bitmap.width); // padding currently not supported\na";
    state->textbox = textbox_create(50, 100, 100, 80, text);
    state->draw = false;    

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    unsigned long flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (!SDL_CreateWindowAndRenderer("Hello World", 960, 540, flags, &state->window, &state->renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state->style = (Style) {
        .font_size = 20,
        .cursor_color = {0xff, 0xff, 0xff, 0xff},
        .line_height = 1,
        .selection_color = {0xe3, 0x88, 0x64, 0xff},
        .text_color = {0xe3, 0x88, 0x64, 0xff},
        .text_selected_color = {0xff, 0xff, 0xff, 0xff}
    };
    state->scale = SDL_GetWindowDisplayScale(state->window);
    renderctx_create(&state->ctx);
    renderctx_load_style(&state->ctx, state->style, state->scale);

    if (!SDL_StartTextInput(state->window)) {
        SDL_Log("Couldn't start text input: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

static bool is_selecting(SDL_Keymod mod) { return mod & SDL_KMOD_SHIFT && !(mod & (SDL_KMOD_ALT | SDL_KMOD_CTRL | SDL_KMOD_MODE | SDL_KMOD_GUI)); }

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    State *state = (State *) appstate;
    FrameBuffer *fb = &state->ctx.fb;
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN: {
            bool shift = is_selecting(event->key.mod);
            bool ctrl = event->key.mod & SDL_KMOD_CTRL && !(event->key.mod & (SDL_KMOD_ALT | SDL_KMOD_SHIFT | SDL_KMOD_MODE | SDL_KMOD_GUI));
            bool ctrlshift = (event->key.mod & SDL_KMOD_CTRL) && event->key.mod & SDL_KMOD_SHIFT && !(event->key.mod & (SDL_KMOD_ALT | SDL_KMOD_MODE | SDL_KMOD_GUI));
            switch (event->key.key) {
                case SDLK_ESCAPE: return SDL_APP_SUCCESS;
                case SDLK_RIGHT:
                    if (ctrl || ctrlshift) tb_next_word(&state->textbox, ctrlshift);
                    else tb_right(&state->textbox, shift);
                    break;
                case SDLK_LEFT:
                    if (ctrl || ctrlshift) tb_prev_word(&state->textbox, ctrlshift);
                    else tb_left(&state->textbox, shift);
                    break;
                case SDLK_DOWN: tb_down(&state->textbox, &state->ctx, shift); break;
                case SDLK_UP: tb_up(&state->textbox, &state->ctx, shift); break;
                case SDLK_HOME: tb_home(&state->textbox, &state->ctx, shift); break;
                case SDLK_END: tb_end(&state->textbox, &state->ctx, shift); break;
                case SDLK_PAGEUP: tb_page_up(&state->textbox, &state->ctx, shift); break;
                case SDLK_PAGEDOWN: tb_page_down(&state->textbox, &state->ctx, shift); break;
                case SDLK_C: if (ctrl) tb_copy(state->textbox); break;
                case SDLK_V: if (ctrl) tb_paste(&state->textbox); break;
                case SDLK_X: if (ctrl) tb_cut(&state->textbox); break;
                case SDLK_A: if (ctrl) tb_select_all(&state->textbox); break;
                case SDLK_BACKSPACE: tb_backspace(&state->textbox); break;
                case SDLK_DELETE: tb_delete(&state->textbox); break;
                case SDLK_KP_ENTER: case SDLK_RETURN: tb_write(&state->textbox, LF); break;
            }
            state->draw = true;
            break;
        }
        case SDL_EVENT_TEXT_INPUT: {
            size_t len, text_len = strlen(event->text.text);
            ensure(!utf8_measure_codepoints(event->text.text, text_len, &len, NULL));
            uint32_t *codepoints = malloc(len * sizeof(uint32_t));
            ensure(codepoints != NULL);
            ensure(!utf8_decode(event->text.text, text_len, true, len, codepoints, NULL));
            tb_write_n(&state->textbox, codepoints, len);
            free(codepoints);
            state->draw = true;
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (event->button.button == SDL_BUTTON_LEFT) {
                int_least32_t x = (int_least32_t) (event->button.x * state->scale + 0.5);
                int_least32_t y = (int_least32_t) (event->button.y * state->scale + 0.5);
                if (tb_hit(state->textbox, x, y)) {
                    state->last_hit = &state->textbox;
                    tb_mouse(&state->textbox, &state->ctx, x, y, is_selecting(event->key.mod));
                    state->draw = true;
                } else state->last_hit = NULL;
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (event->motion.state & SDL_BUTTON_LMASK) {
                int_least32_t x = (int_least32_t) (event->motion.x * state->scale + 0.5);
                int_least32_t y = (int_least32_t) (event->motion.y * state->scale + 0.5);
                if (state->last_hit == &state->textbox) { tb_mouse(&state->textbox, &state->ctx, x, y, true); state->draw = true; }
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            int_least32_t x = (int_least32_t) (event->wheel.mouse_x * state->scale + 0.5);
            int_least32_t y = (int_least32_t) (event->wheel.mouse_y * state->scale + 0.5);
            if (tb_hit(state->textbox, x, y)) {
                int_least32_t scroll = event->wheel.integer_x * fb->image.height / 10;
                state->textbox.scroll_y = min(0, state->textbox.scroll_y + (event->wheel.direction == SDL_MOUSEWHEEL_NORMAL ? scroll : -scroll));
                state->draw = true;
            }
            break;
        }
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            // NOTE: event -> window.data don't hold updated dimensions but what was entered when creating the window
            uint w, h;
            if (!SDL_GetRenderOutputSize(state->renderer, (int *) &w, (int *) &h)) {
                SDL_Log("Couldn't GetRenderOutputSize: %s", SDL_GetError());
                return SDL_APP_FAILURE;
            }

            float scale = SDL_GetWindowDisplayScale(state->window);
            if (scale != state->scale) {
                state->scale = scale;
                renderctx_load_style(&state->ctx, state->style, state->scale);
            }

            bool resize = false;
            // ensure scaling with powers of 2
            if (w > fb->max_width) {
                fb->max_width = ((size_t) 1 << (size_t) ceilf(log2f((float) w / fb->max_width))) * fb->max_width;;
                resize = true;
            }
            if (h > fb->max_height) {
                fb->max_height = ((size_t) 1 << (size_t) ceilf(log2f((float) h / fb->max_height))) * fb->max_height;;
                resize = true;
            }
            if (resize) {
                fb->data = realloc(fb->data, fb->max_width * fb->max_height * get_pixel_data_size(fb->image.format));
                ensure(fb->data != NULL);
            }
            if (resize || fb->texture == NULL) {
                if (fb->texture != NULL) SDL_DestroyTexture(fb->texture);
                fb->texture = SDL_CreateTexture(state->renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, fb->max_width, fb->max_height);
                if (fb->texture == NULL) {
                    SDL_Log("Couldn't CreateTexture: %s", SDL_GetError());
                    return SDL_APP_FAILURE;
                }
            }
            fb->image = image_create_from_data(fb->data, w, h, fb->image.format);
            
            state->textbox.w = w - 100;
            
            state->draw = true;
            break;
        }
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame */
SDL_AppResult SDL_AppIterate(void *appstate) {
    State *state = (State *) appstate;
    FrameBuffer *fb = &state->ctx.fb;
    if (state->draw) {
        image_draw_rect(fb->image, (Rectangle) {0, 0, fb->image.width, fb->image.height}, (Color) {0, 0, 0, 255});

        image_draw_rect(fb->image, (Rectangle) {state->textbox.x - 1,                state->textbox.y - 1,                    state->textbox.w + 1, 1},                    (Color) {255, 255, 255, 255});
        image_draw_rect(fb->image, (Rectangle) {state->textbox.x - 1,                state->textbox.y - 1,                    1,                    state->textbox.h + 2}, (Color) {255, 255, 255, 255});
        image_draw_rect(fb->image, (Rectangle) {state->textbox.x - 1,                state->textbox.y + state->textbox.h + 1, state->textbox.w + 1, 1},                    (Color) {255, 255, 255, 255});
        image_draw_rect(fb->image, (Rectangle) {state->textbox.x + state->textbox.w, state->textbox.y - 1,                    1,                    state->textbox.h + 2}, (Color) {255, 255, 255, 255});
    
        tb_draw(&state->textbox, &state->ctx);

        SDL_Rect framebuffer_rect = {.x = 0, .y = 0, .w = fb->image.width, .h = fb->image.height};
        SDL_UpdateTexture(fb->texture, &framebuffer_rect, fb->image.data, fb->image.width * 3);
        SDL_FRect framebuffer_frect = {.x = 0, .y = 0, .w = (float) fb->image.width, .h = (float) fb->image.height};
        SDL_RenderTexture(state->renderer, fb->texture, &framebuffer_frect, NULL);
        SDL_RenderPresent(state->renderer);
        state->draw = false;
    }

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void) result;
    State *state = (State *) appstate;
    SDL_StopTextInput(state->window);
    gb_destroy(&state->textbox.gb);
    renderctx_destroy(&state->ctx);
    SDL_DestroyTexture(state->ctx.fb.texture);
    SDL_DestroyRenderer(state->renderer);
    SDL_DestroyWindow(state->window);
    free(state);
}
