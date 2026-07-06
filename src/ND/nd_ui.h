#ifndef ND_UI_H
#define ND_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>

/* ND keeps one logical canvas and scales it into the resizable window, same pattern as PFD. */
extern TTF_Font *nd_font10;
extern TTF_Font *nd_font12;
extern TTF_Font *nd_font16;
extern TTF_Font *nd_font20;
extern TTF_Font *nd_font28;

extern int nd_window_width;
extern int nd_window_height;
extern SDL_Texture *nd_logic_texture;
extern SDL_Rect nd_render_rect;

int initND(void);
SDL_Window *createNDWindow(void);
SDL_Renderer *createNDRenderer(SDL_Window *window);
SDL_Texture *createNDLogicTexture(SDL_Renderer *renderer);
void destroyND(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *logic_texture);
void initNDLayout(void);

void drawNDBackground(SDL_Renderer *renderer);
void drawNDMapMode(SDL_Renderer *renderer);
void drawNDStatus(SDL_Renderer *renderer);

#endif
