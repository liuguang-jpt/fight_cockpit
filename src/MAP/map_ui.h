#ifndef MAP_UI_H
#define MAP_UI_H

#include "map_data.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define MAP_WINDOW_WIDTH 1440
#define MAP_WINDOW_HEIGHT 900
#define MAP_TEXTURE_WIDTH 1024
#define MAP_TEXTURE_HEIGHT 768

typedef struct MapLayout {
    SDL_Rect title_rect;
    SDL_Rect city_rect;
    SDL_Rect weather_rect;
    SDL_Rect flight_rect;
    SDL_Rect map_rect;
    SDL_Rect zoom_in_rect;
    SDL_Rect zoom_out_rect;
} MapLayout;

extern MapLayout g_map_layout;

int initMap(void);
SDL_Window *createMapWindow(void);
SDL_Renderer *createMapRenderer(SDL_Window *window);
void destroyMap(SDL_Window *window, SDL_Renderer *renderer);
void initMapLayout(int width, int height);
void map_refresh_dynamic_texture(SDL_Renderer *renderer);
void drawMapScreen(SDL_Renderer *renderer, const MapSnapshot *snapshot);
int map_handle_click(int x, int y);

#endif
