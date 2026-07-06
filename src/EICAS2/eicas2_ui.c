#include "eicas2_ui.h"

#include "eicas2_data.h"
#include <stdio.h>

#define EICAS2_WIDTH 772
#define EICAS2_HEIGHT 721

TTF_Font *eicas2_font12 = NULL;
TTF_Font *eicas2_font16 = NULL;
TTF_Font *eicas2_font20 = NULL;
TTF_Font *eicas2_font28 = NULL;
int eicas2_window_width = EICAS2_WIDTH;
int eicas2_window_height = EICAS2_HEIGHT;
SDL_Texture *eicas2_logic_texture = NULL;
SDL_Rect eicas2_render_rect = {0, 0, EICAS2_WIDTH, EICAS2_HEIGHT};

static const SDL_Color EICAS2_WHITE = {240, 240, 240, 255};
static const SDL_Color EICAS2_GREEN = {80, 236, 124, 255};
static const SDL_Color EICAS2_AMBER = {255, 188, 64, 255};
static const SDL_Color EICAS2_CYAN = {77, 219, 255, 255};

static void render_text(SDL_Renderer *renderer, int x, int y, const char *text, TTF_Font *font, SDL_Color color) {
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;
    SDL_Rect dest;
    if (renderer == NULL || font == NULL || text == NULL || *text == '\0') return;
    surface = TTF_RenderUTF8_Solid(font, text, color);
    if (surface == NULL) return;
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        SDL_FreeSurface(surface);
        return;
    }
    dest.x = x;
    dest.y = y;
    dest.w = surface->w;
    dest.h = surface->h;
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void render_text_centered(SDL_Renderer *renderer, int center_x, int y, const char *text, TTF_Font *font, SDL_Color color) {
    int text_w = 0;
    int text_h = 0;
    if (font == NULL || text == NULL || *text == '\0') return;
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) == -1) return;
    render_text(renderer, center_x - text_w / 2, y, text, font, color);
}

static void draw_column(SDL_Renderer *renderer, int x, const char *title, float v1, float v2, float v3, float v4, float v5, float v6) {
    char text[32];

    render_text_centered(renderer, x, 124, title, eicas2_font20, EICAS2_WHITE);
    render_text_centered(renderer, x - 88, 188, "N2", eicas2_font12, EICAS2_WHITE);
    render_text_centered(renderer, x - 88, 258, "VIB", eicas2_font12, EICAS2_WHITE);
    render_text_centered(renderer, x - 88, 328, "OIL PSI", eicas2_font12, EICAS2_WHITE);
    render_text_centered(renderer, x - 88, 398, "OIL TEMP", eicas2_font12, EICAS2_WHITE);
    render_text_centered(renderer, x - 88, 468, "OIL QTY", eicas2_font12, EICAS2_WHITE);
    render_text_centered(renderer, x - 88, 538, "FF", eicas2_font12, EICAS2_WHITE);

    snprintf(text, sizeof(text), "%5.1f", v1);
    render_text_centered(renderer, x + 28, 182, text, eicas2_font20, EICAS2_GREEN);
    snprintf(text, sizeof(text), "%5.2f", v2);
    render_text_centered(renderer, x + 28, 252, text, eicas2_font20, EICAS2_CYAN);
    snprintf(text, sizeof(text), "%5.1f", v3);
    render_text_centered(renderer, x + 28, 322, text, eicas2_font20, EICAS2_GREEN);
    snprintf(text, sizeof(text), "%5.1f", v4);
    render_text_centered(renderer, x + 28, 392, text, eicas2_font20, EICAS2_AMBER);
    snprintf(text, sizeof(text), "%5.1f", v5);
    render_text_centered(renderer, x + 28, 462, text, eicas2_font20, EICAS2_GREEN);
    snprintf(text, sizeof(text), "%5.2f", v6);
    render_text_centered(renderer, x + 28, 532, text, eicas2_font20, EICAS2_CYAN);
}

int initEICAS2(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) return -1;
    if (TTF_Init() == -1) return -1;
    eicas2_font12 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 12);
    eicas2_font16 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 16);
    eicas2_font20 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 20);
    eicas2_font28 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 28);
    if (eicas2_font12 == NULL || eicas2_font16 == NULL || eicas2_font20 == NULL || eicas2_font28 == NULL) return -1;
    if (IMG_Init(IMG_INIT_PNG) == -1) return -1;
    return 0;
}

SDL_Window *createEICAS2Window(void) {
    return SDL_CreateWindow("EICAS2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, eicas2_window_width, eicas2_window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
}

SDL_Renderer *createEICAS2Renderer(SDL_Window *window) {
    return SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
}

SDL_Texture *createEICAS2LogicTexture(SDL_Renderer *renderer) {
    return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, EICAS2_WIDTH, EICAS2_HEIGHT);
}

void destroyEICAS2(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *logic_texture) {
    if (eicas2_font12 != NULL) TTF_CloseFont(eicas2_font12);
    if (eicas2_font16 != NULL) TTF_CloseFont(eicas2_font16);
    if (eicas2_font20 != NULL) TTF_CloseFont(eicas2_font20);
    if (eicas2_font28 != NULL) TTF_CloseFont(eicas2_font28);
    if (logic_texture != NULL) SDL_DestroyTexture(logic_texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void drawEICAS2Background(SDL_Renderer *renderer) {
    boxRGBA(renderer, 0, 0, EICAS2_WIDTH, EICAS2_HEIGHT, 0, 0, 0, 255);
    roundedRectangleRGBA(renderer, 26, 18, 746, 695, 18, 120, 130, 140, 255);
    render_text(renderer, 44, 36, "ENGINE SECONDARY", eicas2_font20, EICAS2_WHITE);
}

void drawEICAS2Page(SDL_Renderer *renderer) {
    draw_column(renderer, 252, "ENG 1", eicas2_n2_left, eicas2_vib_left, eicas2_oil_pressure_left, eicas2_oil_temp_left, eicas2_oil_qty_left, eicas2_fuel_flow_left);
    draw_column(renderer, 520, "ENG 2", eicas2_n2_right, eicas2_vib_right, eicas2_oil_pressure_right, eicas2_oil_temp_right, eicas2_oil_qty_right, eicas2_fuel_flow_right);
}

void drawEICAS2Status(SDL_Renderer *renderer) {
    const char *source = "STATIC";
    const char *status = eicas2_xplane_connected ? "UDP OK" : "SIM";

    if (eicas2_data_source == EICAS2_SOURCE_FILE) source = "FILE";
    else if (eicas2_data_source == EICAS2_SOURCE_XPLANE) source = "XPLANE";
    render_text(renderer, 628, 34, source, eicas2_font16, EICAS2_GREEN);
    render_text(renderer, 628, 54, status, eicas2_font12, eicas2_xplane_connected ? EICAS2_GREEN : EICAS2_AMBER);
}
