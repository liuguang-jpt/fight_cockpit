#include <stdlib.h>

#include "map_data.h"
#include "map_ui.h"

static void capture_frame_if_requested(SDL_Renderer *renderer, int width, int height) {
    const char *capture_path = getenv("MAP_CAPTURE_PATH");
    SDL_Surface *surface = NULL;

    if (capture_path == NULL || *capture_path == '\0') return;
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
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
    MapSnapshot snapshot;
    Uint32 start_ticks = 0;
    int window_width = MAP_WINDOW_WIDTH;
    int window_height = MAP_WINDOW_HEIGHT;
    int quit = 0;

    (void)argc;
    (void)argv;

    if (initMap() == -1) return -1;
    if (map_init_data() == -1) {
        destroyMap(NULL, NULL);
        return -1;
    }

    window = createMapWindow();
    if (window == NULL) {
        map_shutdown_data();
        destroyMap(window, NULL);
        return -1;
    }

    renderer = createMapRenderer(window);
    if (renderer == NULL) {
        map_shutdown_data();
        destroyMap(window, renderer);
        return -1;
    }

    SDL_GetWindowSize(window, &window_width, &window_height);
    initMapLayout(window_width, window_height);
    start_ticks = SDL_GetTicks();

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_EQUALS) map_request_zoom_delta(1);
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_MINUS) map_request_zoom_delta(-1);

            if (event.type == SDL_MOUSEWHEEL) {
                if (event.wheel.y > 0) map_request_zoom_delta(1);
                if (event.wheel.y < 0) map_request_zoom_delta(-1);
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int delta = map_handle_click(event.button.x, event.button.y);
                if (delta != 0) map_request_zoom_delta(delta);
            }

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                window_width = event.window.data1;
                window_height = event.window.data2;
                initMapLayout(window_width, window_height);
            }
        }

        map_get_snapshot(&snapshot);
        map_refresh_dynamic_texture(renderer);
        drawMapScreen(renderer, &snapshot);
        if (getenv("MAP_CAPTURE_PATH") != NULL && SDL_GetTicks() - start_ticks > 1200) capture_frame_if_requested(renderer, window_width, window_height);
        SDL_RenderPresent(renderer);

        if (getenv("MAP_CAPTURE_PATH") != NULL && SDL_GetTicks() - start_ticks > 1200) quit = 1;
        SDL_Delay(16);
    }

    map_shutdown_data();
    destroyMap(window, renderer);
    return 0;
}
