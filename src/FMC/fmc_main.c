#include <stdlib.h>

#include "fmc_data.h"
#include "fmc_ui.h"

/* Keep the fixed 638x998 logic surface centered while the window resizes. */
static void update_render_rect(int width, int height) {
    float scale_x = (float)width / (float)FMC_WIDTH;
    float scale_y = (float)height / (float)FMC_HEIGHT;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    int render_w = (int)((float)FMC_WIDTH * scale);
    int render_h = (int)((float)FMC_HEIGHT * scale);

    fmc_render_rect.x = (width - render_w) / 2;
    fmc_render_rect.y = (height - render_h) / 2;
    fmc_render_rect.w = render_w;
    fmc_render_rect.h = render_h;
}

static int window_to_logic_x(int window_x) {
    if (fmc_render_rect.w <= 0) return 0;
    return (window_x - fmc_render_rect.x) * FMC_WIDTH / fmc_render_rect.w;
}

static int window_to_logic_y(int window_y) {
    if (fmc_render_rect.h <= 0) return 0;
    return (window_y - fmc_render_rect.y) * FMC_HEIGHT / fmc_render_rect.h;
}

static int point_in_render_rect(int x, int y) {
    return x >= fmc_render_rect.x && x < fmc_render_rect.x + fmc_render_rect.w &&
        y >= fmc_render_rect.y && y < fmc_render_rect.y + fmc_render_rect.h;
}

/* One-shot capture lets us verify rendering without waiting for manual input. */
static void capture_frame_if_requested(SDL_Renderer *renderer) {
    const char *capture_path = getenv("FMC_CAPTURE_PATH");
    SDL_Surface *surface = NULL;

    if (capture_path == NULL || *capture_path == '\0') return;
    surface = SDL_CreateRGBSurfaceWithFormat(0, FMC_WIDTH, FMC_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL) return;
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA32, surface->pixels, surface->pitch) == -1) {
        SDL_FreeSurface(surface);
        return;
    }
    SDL_SaveBMP(surface, capture_path);
    SDL_FreeSurface(surface);
}

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Event event;
    int quit = 0;

    (void)argc;
    (void)argv;

    if (initFMC() == -1) return -1;
    window = createFMCWindow();
    if (window == NULL) {
        destroyFMC(window, NULL, NULL);
        return -1;
    }

    renderer = createFMCRenderer(window);
    if (renderer == NULL) {
        destroyFMC(window, renderer, NULL);
        return -1;
    }

    fmc_logic_texture = createFMCLogicTexture(renderer);
    if (fmc_logic_texture == NULL) {
        destroyFMC(window, renderer, fmc_logic_texture);
        return -1;
    }

    fmc_init_state();
    fmc_load_navigation_data(NULL, NULL, NULL);
    init_fmc_buttons();
    update_render_rect(fmc_window_width, fmc_window_height);
    SDL_StartTextInput();

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) fmc_handle_backspace();
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN) fmc_handle_enter();
            if (event.type == SDL_TEXTINPUT) fmc_handle_text_input(event.text.text);

            if (event.type == SDL_MOUSEMOTION) {
                if (point_in_render_rect(event.motion.x, event.motion.y))
                    fmc_update_hover(window_to_logic_x(event.motion.x), window_to_logic_y(event.motion.y));
                else
                    fmc_update_hover(-1, -1);
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (point_in_render_rect(event.button.x, event.button.y))
                    fmc_click_button_at(window_to_logic_x(event.button.x), window_to_logic_y(event.button.y));
            }

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                fmc_window_width = event.window.data1;
                fmc_window_height = event.window.data2;
                update_render_rect(fmc_window_width, fmc_window_height);
            }
        }

        SDL_SetRenderTarget(renderer, fmc_logic_texture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        draw_fmc(renderer);

        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, fmc_logic_texture, NULL, &fmc_render_rect);
        capture_frame_if_requested(renderer);
        SDL_RenderPresent(renderer);

        if (getenv("FMC_CAPTURE_PATH") != NULL) quit = 1;
        SDL_Delay(16);
    }

    SDL_StopTextInput();
    fmc_free_navigation_data();
    destroyFMC(window, renderer, fmc_logic_texture);
    return 0;
}
