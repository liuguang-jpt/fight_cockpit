#ifndef PFD_UI_H
#define PFD_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>

//定义多种大小的绘制字体
extern TTF_Font *font8; 
extern TTF_Font *font12;
extern TTF_Font *font16;
extern TTF_Font *font20;
extern TTF_Font *font24;

//定义窗口的高和宽
extern int window_width;
extern int window_height;

//逻辑纹理
extern SDL_Texture *logic_texture;

//缩放后的渲染区域
extern SDL_Rect g_render_rect;

//创建界面
int initPFD();

//创建窗口
SDL_Window* createPFDWindow(void);

//创建渲染器
SDL_Renderer* createPFDRenderer(SDL_Window* window);

//清除资源
void destroyPFD(SDL_Window* window,SDL_Renderer* renderer,SDL_Texture *logic_texture);

//创建逻辑纹理
SDL_Texture* createLogicTexture(SDL_Renderer* renderer);

#endif
