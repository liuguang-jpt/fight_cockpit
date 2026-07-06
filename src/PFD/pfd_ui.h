#ifndef PFD_UI_H
#define PFD_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>

extern TTF_Font *font8;
extern TTF_Font *font12;
extern TTF_Font *font16;
extern TTF_Font *font20;
extern TTF_Font *font24;

extern int window_width;
extern int window_height;
extern SDL_Texture *logic_texture;
extern SDL_Rect g_render_rect;

int initPFD(void);
SDL_Window *createPFDWindow(void);
SDL_Renderer *createPFDRenderer(SDL_Window *window);
SDL_Texture *createLogicTexture(SDL_Renderer *renderer);
void destroyPFD(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *logic_texture);
void initPFD_Layout(void);

void drawFlightModeAnnunciator(SDL_Renderer *renderer);
void drawAirspeedIndicator(SDL_Renderer *renderer);
void drawAltitudeIndicator(SDL_Renderer *renderer);
void drawAttitudeIndicator(SDL_Renderer *renderer, float pitch, float roll);
void drawVerticalSpeedIndicator(SDL_Renderer *renderer);
void drawHeadingIndicator(SDL_Renderer *renderer);
void drawThrustIndicator(SDL_Renderer *renderer);
void drawPFDStatus(SDL_Renderer *renderer);

#endif
