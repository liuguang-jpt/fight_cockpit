#include <stdio.h>
#include <stdlib.h>

#include "nd_data.h"
#include "nd_ui.h"

static void update_render_rect(int width, int height) {
    float scale_x = (float)width / 772.0f;
    float scale_y = (float)height / 721.0f;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    int render_w = (int)(772.0f * scale);
    int render_h = (int)(721.0f * scale);

    nd_render_rect.x = (width - render_w) / 2;
    nd_render_rect.y = (height - render_h) / 2;
    nd_render_rect.w = render_w;
    nd_render_rect.h = render_h;
}

static void capture_frame_if_requested(SDL_Renderer *renderer) {
    const char *capture_path = getenv("ND_CAPTURE_PATH");
    SDL_Surface *surface = NULL;

    if (capture_path == NULL || *capture_path == '\0') return;
    surface = SDL_CreateRGBSurfaceWithFormat(0, 772, 721, 32, SDL_PIXELFORMAT_RGBA32);
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
    HANDLE data_thread = NULL;
    NDThreadContext thread_ctx;
    NDData initial_data;
    int quit = 0;

    (void)argc;
    (void)argv;

    if (initND() == -1) return -1;
    window = createNDWindow();
    if (window == NULL) {
        destroyND(window, NULL, NULL);
        return -1;
    }

    renderer = createNDRenderer(window);
    if (renderer == NULL) {
        destroyND(window, renderer, NULL);
        return -1;
    }

    nd_logic_texture = createNDLogicTexture(renderer);
    if (nd_logic_texture == NULL) {
        destroyND(window, renderer, nd_logic_texture);
        return -1;
    }

    thread_ctx.mode = get_nd_data_mode_from_env();
    thread_ctx.xp_port = get_nd_xplane_port_from_env();
    thread_ctx.file_path = ND_DATA_FILE_PATH;

    init_nd_defaults(&initial_data);
    if (thread_ctx.mode != ND_MODE_XPLANE) {
        FILE *bootstrap = open_nd_data_file(thread_ctx.file_path);
        if (bootstrap != NULL) {
            /* Bootstrap one file frame so the first paint already shows live-looking ND values. */
            load_next_nd_frame(bootstrap, &initial_data);
            fclose(bootstrap);
        }
    }

    g_shared_nd_data = initial_data;
    sync_globals_from_nd_data(&g_shared_nd_data);
    data_thread = CreateThread(NULL, 0, nd_data_thread_proc, &thread_ctx, 0, NULL);
    update_render_rect(nd_window_width, nd_window_height);

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                nd_window_width = event.window.data1;
                nd_window_height = event.window.data2;
                update_render_rect(nd_window_width, nd_window_height);
                initNDLayout();
            }
        }

        if (InterlockedCompareExchange(&g_nd_data_ready, 0, 0) == 1) {
            NDData latest_data = g_shared_nd_data;
            /* The UI thread copies one complete snapshot at a time, then releases the worker to publish the next one. */
            sync_globals_from_nd_data(&latest_data);
            InterlockedExchange(&g_nd_data_ready, 0);
        }

        SDL_SetRenderTarget(renderer, nd_logic_texture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        drawNDBackground(renderer);
        drawNDMapMode(renderer);
        drawNDStatus(renderer);

        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, nd_logic_texture, NULL, &nd_render_rect);
        capture_frame_if_requested(renderer);
        SDL_RenderPresent(renderer);

        if (getenv("ND_CAPTURE_PATH") != NULL) quit = 1;
        SDL_Delay(16);
    }

    InterlockedExchange(&g_nd_exit_requested, 1);
    if (data_thread != NULL) {
        WaitForSingleObject(data_thread, 1500);
        CloseHandle(data_thread);
    }

    destroyND(window, renderer, nd_logic_texture);
    return 0;
}
