#ifndef EICAS2_UI_H
#define EICAS2_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>

extern TTF_Font *eicas2_font12;
extern TTF_Font *eicas2_font16;
extern TTF_Font *eicas2_font20;
extern TTF_Font *eicas2_font28;

extern int eicas2_window_width;
extern int eicas2_window_height;
extern SDL_Texture *eicas2_logic_texture;
extern SDL_Rect eicas2_render_rect;

int initEICAS2(void);
SDL_Window *createEICAS2Window(void);
SDL_Renderer *createEICAS2Renderer(SDL_Window *window);
SDL_Texture *createEICAS2LogicTexture(SDL_Renderer *renderer);
void destroyEICAS2(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *logic_texture);
void drawEICAS2Background(SDL_Renderer *renderer);
void drawEICAS2Page(SDL_Renderer *renderer);
void drawEICAS2Status(SDL_Renderer *renderer);

#endif
