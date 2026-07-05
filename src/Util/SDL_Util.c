#include "SDL_Util.h"
TTF_Font *font8 = NULL;
TTF_Font *font12 = NULL;
TTF_Font *font16 = NULL;
TTF_Font *font20 = NULL;
TTF_Font *font24 = NULL;
int initSDL()
{
    // 鍒濆鍖朣DL
    if (SDL_Init(SDL_INIT_VIDEO) == -1)
    {
        printf("SDL鍒濆鍖栧け璐? %s\n", SDL_GetError());
        return -1;
    }
    // 鍒濆鍖朤TF
    if (TTF_Init() == -1)
    {
        printf("SDL 瀛椾綋鍒濆鍖栧け璐? %s\n", TTF_GetError());
        return -1;
    }
    else
    {
        font8 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 8);
        font12 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 12);
        font16 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 16);
        font20 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 20);
        font24 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 24);
        if (font8 == NULL || font12 == NULL || font16 == NULL || font20 == NULL || font24 == NULL)
        {
            printf("SDL 瀛椾綋鍒濆鍖栧け璐? %s\n", TTF_GetError());
            return -1;
        }
    }
    if (IMG_Init(IMG_INIT_PNG) == -1)
    {
        printf("SDL 鍥剧墖鍒濆鍖栧け璐? %s\n", IMG_GetError());
        return -1;
    }
    return 0;
}
void renderText(SDL_Renderer *renderer, SDL_Color color, TTF_Font *font, int x, int y, const char *text)
{
    // 鍒ゆ柇鍙傛暟鏄惁涓篘ULL
    if (!font || !text)
        return;
    // 鏍规嵁瀛椾綋鍒涘缓涓€涓〃闈紝鐒跺悗鍐嶇敱琛ㄩ潰鍒涘缓涓€涓汗鐞?
    SDL_Surface *surface = TTF_RenderUTF8_Solid(font, text, color);
    if (!surface)
    {
        printf("TTF_RenderText_Solid error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        printf("SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }
    // 灏嗙汗鐞嗙粯鍒跺埌娓叉煋鍣ㄤ笂
    SDL_Rect dest_rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
    // 閲婃斁璧勬簮
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

int destroySDL(SDL_Renderer *renderer, SDL_Window *window)
{
    // 清理字体
    TTF_CloseFont(font8);
    TTF_CloseFont(font12);
    TTF_CloseFont(font16);
    TTF_CloseFont(font20);
    TTF_CloseFont(font24);
    font8 = NULL; font12 = NULL; font16 = NULL; font20 = NULL; font24 = NULL;
    // 退出SDL
    TTF_Quit();
    IMG_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDL_Quit();
    return 0;
}
