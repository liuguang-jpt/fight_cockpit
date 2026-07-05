#include "Util/SDL_Util.h"
int main(int argc, char *argv[]) {

    if(initSDL() == -1)
    {
        printf("SDL加载失败\n");
    }
    else
    {
        printf("SDL加载成功\n");
    }
     //创建可调整大小的窗口
    SDL_Window *window = SDL_CreateWindow(
        "TEST",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 800,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (window == NULL)
    {
        printf("创建窗口失败！\n");
        return -1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    int quit = 0;
    SDL_Event event;
    while (!quit)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // 点（红色）
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderDrawPoint(renderer, 50, 50);

        // 线段（蓝色）
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderDrawLine(renderer, 100, 100, 200, 200);

        // 空心矩形（绿色）
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_Rect rect = {200, 200, 100, 100};
        SDL_RenderDrawRect(renderer, &rect);

        // 圆弧（紫色）
        arcRGBA(renderer, 150, 250, 70, 0, 270, 128, 0, 128, 255);
        // 扇形（橙色）
        pieRGBA(renderer, 350, 250, 70, 0, 120, 255, 165, 0, 255);
        // 实心三角形（青色）
        filledPolygonRGBA(renderer,
            (Sint16[]){500, 580, 540},
            (Sint16[]){220, 220, 140},
            3, 0, 255, 255, 255);
        // 白色文字
        renderText(renderer, (SDL_Color){255, 255, 255, 255}, font20, 10, 10, "Hello SDL2!");

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    destroySDL(renderer, window);
    return 0;
}

