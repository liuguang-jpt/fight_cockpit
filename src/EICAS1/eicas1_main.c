#include <stdlib.h>

#include "eicas1_data.h"
#include "eicas1_ui.h"

static void update_render_rect(int width, int height) {
    float scale_x = (float)width / 772.0f;
    float scale_y = (float)height / 721.0f;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    int render_w = (int)(772.0f * scale);
    int render_h = (int)(721.0f * scale);

    eicas1_render_rect.x = (width - render_w) / 2;
    eicas1_render_rect.y = (height - render_h) / 2;
    eicas1_render_rect.w = render_w;
    eicas1_render_rect.h = render_h;
}

static void capture_frame_if_requested(SDL_Renderer *renderer) {
    const char *capture_path = getenv("EICAS1_CAPTURE_PATH");
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
    EICAS1ThreadContext thread_ctx;
    EICAS1Data initial_data;
    int quit = 0;

    (void)argc;
    (void)argv;

    if (initEICAS1() == -1) return -1;
    window = createEICAS1Window();
    if (window == NULL) {
        destroyEICAS1(window, NULL, NULL);
        return -1;
    }

    renderer = createEICAS1Renderer(window);
    if (renderer == NULL) {
        destroyEICAS1(window, renderer, NULL);
        return -1;
    }

    eicas1_logic_texture = createEICAS1LogicTexture(renderer);
    if (eicas1_logic_texture == NULL) {
        destroyEICAS1(window, renderer, eicas1_logic_texture);
        return -1;
    }

    thread_ctx.mode = get_eicas1_data_mode_from_env();
    thread_ctx.xp_port = get_eicas1_xplane_port_from_env();
    thread_ctx.file_path = EICAS1_DATA_FILE_PATH;
    init_eicas1_defaults(&initial_data);
    if (thread_ctx.mode != EICAS1_MODE_XPLANE) {
        FILE *bootstrap = open_eicas1_data_file(thread_ctx.file_path);
        if (bootstrap != NULL) {
            load_next_eicas1_frame(bootstrap, &initial_data);
            fclose(bootstrap);
        }
    }
    g_shared_eicas1_data = initial_data;
    sync_globals_from_eicas1_data(&g_shared_eicas1_data);
    data_thread = CreateThread(NULL, 0, eicas1_data_thread_proc, &thread_ctx, 0, NULL);
    update_render_rect(eicas1_window_width, eicas1_window_height);

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                eicas1_window_width = event.window.data1;
                eicas1_window_height = event.window.data2;
                update_render_rect(eicas1_window_width, eicas1_window_height);
            }
        }

        if (InterlockedCompareExchange(&g_eicas1_data_ready, 0, 0) == 1) {
            EICAS1Data latest_data = g_shared_eicas1_data;
            sync_globals_from_eicas1_data(&latest_data);
            InterlockedExchange(&g_eicas1_data_ready, 0);
        }

        SDL_SetRenderTarget(renderer, eicas1_logic_texture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        drawEICAS1Background(renderer);
        drawEICAS1Primary(renderer);
        drawEICAS1Fuel(renderer);
        drawEICAS1Status(renderer);

        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, eicas1_logic_texture, NULL, &eicas1_render_rect);
        capture_frame_if_requested(renderer);
        SDL_RenderPresent(renderer);
        if (getenv("EICAS1_CAPTURE_PATH") != NULL) quit = 1;
        SDL_Delay(16);
    }

    InterlockedExchange(&g_eicas1_exit_requested, 1);
    if (data_thread != NULL) {
        WaitForSingleObject(data_thread, 1500);
        CloseHandle(data_thread);
    }
    destroyEICAS1(window, renderer, eicas1_logic_texture);
    return 0;
}
