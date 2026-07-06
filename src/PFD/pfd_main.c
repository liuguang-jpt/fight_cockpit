#include <stdio.h>
#include <stdlib.h>
#include "pfd_data.h"
#include "pfd_ui.h"

static void update_render_rect(int width, int height) {
    float scale_x = (float)width / 772.0f;
    float scale_y = (float)height / 721.0f;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    int render_w = (int)(772.0f * scale);
    int render_h = (int)(721.0f * scale);

    g_render_rect.x = (width - render_w) / 2;
    g_render_rect.y = (height - render_h) / 2;
    g_render_rect.w = render_w;
    g_render_rect.h = render_h;
}

static void capture_frame_if_requested(SDL_Renderer *renderer) {
    const char *capture_path = getenv("PFD_CAPTURE_PATH");
    SDL_Surface *surface = NULL;

    if (capture_path == NULL || *capture_path == '\0') {
        return;
    }

    surface = SDL_CreateRGBSurfaceWithFormat(0, 772, 721, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL) {
        printf("创建截图缓冲失败: %s\n", SDL_GetError());
        return;
    }

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA32, surface->pixels, surface->pitch) == -1) {
        printf("读取渲染像素失败: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    if (SDL_SaveBMP(surface, capture_path) == -1) {
        printf("保存截图失败: %s\n", SDL_GetError());
    }
    SDL_FreeSurface(surface);
}

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Event event;
    HANDLE data_thread = NULL;
    PFDThreadContext thread_ctx;
    PFDData initial_data;
    int quit = 0;

    (void)argc;
    (void)argv;

    if (initPFD() == -1) {
        return -1;
    }
    printf("PFD库初始化成功\n");

    window = createPFDWindow();
    if (window == NULL) {
        destroyPFD(window, NULL, NULL);
        return -1;
    }

    renderer = createPFDRenderer(window);
    if (renderer == NULL) {
        destroyPFD(window, renderer, NULL);
        return -1;
    }

    logic_texture = createLogicTexture(renderer);
    if (logic_texture == NULL) {
        destroyPFD(window, renderer, logic_texture);
        return -1;
    }

    thread_ctx.mode = get_pfd_data_mode_from_env();
    thread_ctx.xp_port = get_xplane_port_from_env();
    thread_ctx.file_path = PFD_DATA_FILE_PATH;

    init_pfd_defaults(&initial_data);
    if (thread_ctx.mode != PFD_MODE_XPLANE) {
        // 先同步一帧文件数据，避免窗口刚打开时先看到默认静态值再跳到文件值。
        FILE *bootstrap = open_pfd_data_file(thread_ctx.file_path);
        if (bootstrap != NULL) {
            load_next_file_frame(bootstrap, &initial_data);
            fclose(bootstrap);
        }
    }
    g_shared_pfd_data = initial_data;
    sync_globals_from_pfd_data(&g_shared_pfd_data);

    data_thread = CreateThread(NULL, 0, pfd_data_thread_proc, &thread_ctx, 0, NULL);
    if (data_thread == NULL) {
        printf("数据线程创建失败，使用静态数据运行\n");
    } else {
        printf("数据线程已启动，模式=%d，端口=%u\n", (int)thread_ctx.mode, (unsigned int)thread_ctx.xp_port);
    }

    update_render_rect(window_width, window_height);

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                window_width = event.window.data1;
                window_height = event.window.data2;
                update_render_rect(window_width, window_height);
                initPFD_Layout();
            }
        }

        if (InterlockedCompareExchange(&g_pfd_data_ready, 0, 0) == 1) {
            // 主线程只复制已经“封箱完成”的共享快照，复制完立刻清标志位，交回给数据线程继续写下一帧。
            PFDData latest_data = g_shared_pfd_data;
            sync_globals_from_pfd_data(&latest_data);
            InterlockedExchange(&g_pfd_data_ready, 0);
        }

        SDL_SetRenderTarget(renderer, logic_texture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        drawFlightModeAnnunciator(renderer);
        drawPFDStatus(renderer);
        drawAttitudeIndicator(renderer, current_pitch, 0.0f - current_roll);
        drawAirspeedIndicator(renderer);
        drawAltitudeIndicator(renderer);
        drawVerticalSpeedIndicator(renderer);
        drawHeadingIndicator(renderer);
        drawThrustIndicator(renderer);

        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, logic_texture, NULL, &g_render_rect);
        capture_frame_if_requested(renderer);
        SDL_RenderPresent(renderer);

        if (getenv("PFD_CAPTURE_PATH") != NULL) {
            quit = 1;
        }
        SDL_Delay(16);
    }

    InterlockedExchange(&g_pfd_exit_requested, 1);
    if (data_thread != NULL) {
        WaitForSingleObject(data_thread, 1500);
        CloseHandle(data_thread);
    }

    destroyPFD(window, renderer, logic_texture);
    printf("PFD程序正常退出\n");
    return 0;
}
