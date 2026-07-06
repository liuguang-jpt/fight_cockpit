#ifndef EICAS1_UI_H
#define EICAS1_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>

extern TTF_Font *eicas1_font12;
extern TTF_Font *eicas1_font16;
extern TTF_Font *eicas1_font20;
extern TTF_Font *eicas1_font28;

extern int eicas1_window_width;
extern int eicas1_window_height;
extern SDL_Texture *eicas1_logic_texture;
extern SDL_Rect eicas1_render_rect;

int initEICAS1(void);
SDL_Window *createEICAS1Window(void);
SDL_Renderer *createEICAS1Renderer(SDL_Window *window);
SDL_Texture *createEICAS1LogicTexture(SDL_Renderer *renderer);
void destroyEICAS1(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *logic_texture);
void drawEICAS1Background(SDL_Renderer *renderer);
void drawEICAS1Primary(SDL_Renderer *renderer);
void drawEICAS1Fuel(SDL_Renderer *renderer);
void drawEICAS1Status(SDL_Renderer *renderer);

#endif
