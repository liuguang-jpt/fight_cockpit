#include "pfd_ui.h"

int window_width=772;
int window_height=721;
TTF_Font *font8 = NULL;
TTF_Font *font12 = NULL;
TTF_Font *font16 = NULL;
TTF_Font *font20 = NULL;
TTF_Font *font24 = NULL;
SDL_Rect g_render_rect={0,0,772,721};
SDL_Texture *logic_texture=NULL;


//界面创建
int initPFD(){
    // 初始化PFD的SDL
    if (SDL_Init(SDL_INIT_VIDEO)==-1){
        printf("PFD的SDL初始化失败: %s\n", SDL_GetError());
        return -1;
    }
    // 初始化TTF
    if (TTF_Init()==-1){
        printf("PFD的SDL字体初始化失败: %s\n", TTF_GetError());
        return -1;
    }else{
        font8 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 8);
        font12 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 12);
        font16 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 16);
        font20 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 20);
        font24 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 24);
        if (font8==NULL||font12==NULL||font16==NULL||font20==NULL||font24==NULL){
            printf("PFD的SDL字体初始化失败: %s\n", TTF_GetError());
            return -1;
        }
    }
    if (IMG_Init(IMG_INIT_PNG)==-1){
        printf("PFD的SDL图片初始化失败: %s\n", IMG_GetError());
        return -1;
    }
    return 0;
}

//窗口创建
SDL_Window* createPFDWindow(){
    SDL_Window* window=SDL_CreateWindow("PFD",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        window_width,window_height,
        SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);

    if (window==NULL){
        printf("PFD窗口创建失败: %s\n",SDL_GetError());
    }
    return window;
}

//渲染器创建
SDL_Renderer* createPFDRenderer(SDL_Window* window){
    SDL_Renderer* renderer=SDL_CreateRenderer(window,-1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);

    if (renderer==NULL){
        printf("PFD渲染器创建失败: %s\n",SDL_GetError());
    }
    return renderer;
}

//逻辑纹理创建
SDL_Texture* createLogicTexture(SDL_Renderer* renderer){
    SDL_Texture* logic_texture=SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        772,721);

    if(logic_texture==NULL){
        printf("逻辑纹理创建失败: %s\n",SDL_GetError());
    }
    return logic_texture;
}

//资源释放
void destroyPFD(SDL_Window* window,SDL_Renderer* renderer,SDL_Texture *logic_texture){
    TTF_Quit();
    IMG_Quit();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if(logic_texture) SDL_DestroyTexture(logic_texture);
    SDL_Quit();
}




