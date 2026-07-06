#include "eicas1_ui.h"

#include "eicas1_data.h"
#include <math.h>
#include <stdio.h>

#define EICAS1_WIDTH 772
#define EICAS1_HEIGHT 721
#define EICAS1_PI 3.14159265f

TTF_Font *eicas1_font12 = NULL;
TTF_Font *eicas1_font16 = NULL;
TTF_Font *eicas1_font20 = NULL;
TTF_Font *eicas1_font28 = NULL;
int eicas1_window_width = EICAS1_WIDTH;
int eicas1_window_height = EICAS1_HEIGHT;
SDL_Texture *eicas1_logic_texture = NULL;
SDL_Rect eicas1_render_rect = {0, 0, EICAS1_WIDTH, EICAS1_HEIGHT};

static const SDL_Color EICAS1_WHITE = {240, 240, 240, 255};
static const SDL_Color EICAS1_GREEN = {80, 236, 124, 255};
static const SDL_Color EICAS1_AMBER = {255, 188, 64, 255};
static const SDL_Color EICAS1_CYAN = {77, 219, 255, 255};

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

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void draw_engine_gauge(SDL_Renderer *renderer, int center_x, int top_y, float n1_value, float egt_value, float ff_value, const char *label) {
    float n1_angle = 210.0f + clamp_float(n1_value / 110.0f, 0.0f, 1.0f) * 120.0f;
    int bar_h = (int)(clamp_float(egt_value / 1000.0f, 0.0f, 1.0f) * 170.0f + 0.5f);
    int end_x = center_x + (int)(cosf(n1_angle * EICAS1_PI / 180.0f) * 62.0f + 0.5f);
    int end_y = top_y + 68 + (int)(sinf(n1_angle * EICAS1_PI / 180.0f) * 62.0f + 0.5f);
    char value[32];
    int tick;

    render_text_centered(renderer, center_x, top_y - 24, label, eicas1_font16, EICAS1_WHITE);
    circleRGBA(renderer, center_x, top_y + 68, 72, 160, 170, 180, 255);
    circleRGBA(renderer, center_x, top_y + 68, 54, 70, 80, 90, 255);

    for (tick = 0; tick <= 10; ++tick) {
        float degrees = 210.0f + tick * 12.0f;
        float radians = degrees * EICAS1_PI / 180.0f;
        int x1 = center_x + (int)(cosf(radians) * 72.0f + 0.5f);
        int y1 = top_y + 68 + (int)(sinf(radians) * 72.0f + 0.5f);
        int x2 = center_x + (int)(cosf(radians) * 58.0f + 0.5f);
        int y2 = top_y + 68 + (int)(sinf(radians) * 58.0f + 0.5f);
        lineRGBA(renderer, x1, y1, x2, y2, 255, 255, 255, 255);
    }

    aalineRGBA(renderer, center_x, top_y + 68, end_x, end_y, 80, 236, 124, 255);
    filledCircleRGBA(renderer, center_x, top_y + 68, 4, 80, 236, 124, 255);
    snprintf(value, sizeof(value), "%4.1f", n1_value);
    render_text_centered(renderer, center_x, top_y + 92, value, eicas1_font20, EICAS1_GREEN);
    render_text_centered(renderer, center_x, top_y + 118, "N1", eicas1_font12, EICAS1_WHITE);

    boxRGBA(renderer, center_x - 85, top_y + 154, center_x - 63, top_y + 324, 28, 30, 34, 255);
    rectangleRGBA(renderer, center_x - 85, top_y + 154, center_x - 63, top_y + 324, 180, 180, 180, 255);
    boxRGBA(renderer, center_x - 84, top_y + 324 - bar_h, center_x - 64, top_y + 323, 255, 188, 64, 255);
    snprintf(value, sizeof(value), "%3.0f", egt_value);
    render_text(renderer, center_x - 108, top_y + 330, "EGT", eicas1_font12, EICAS1_WHITE);
    render_text(renderer, center_x - 101, top_y + 348, value, eicas1_font16, EICAS1_AMBER);

    snprintf(value, sizeof(value), "%4.2f", ff_value);
    render_text(renderer, center_x + 32, top_y + 176, "FF", eicas1_font12, EICAS1_WHITE);
    render_text(renderer, center_x + 28, top_y + 196, value, eicas1_font16, EICAS1_CYAN);
}

int initEICAS1(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) return -1;
    if (TTF_Init() == -1) return -1;
    eicas1_font12 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 12);
    eicas1_font16 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 16);
    eicas1_font20 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 20);
    eicas1_font28 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 28);
    if (eicas1_font12 == NULL || eicas1_font16 == NULL || eicas1_font20 == NULL || eicas1_font28 == NULL) return -1;
    if (IMG_Init(IMG_INIT_PNG) == -1) return -1;
    return 0;
}

SDL_Window *createEICAS1Window(void) {
    return SDL_CreateWindow("EICAS1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, eicas1_window_width, eicas1_window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
}

SDL_Renderer *createEICAS1Renderer(SDL_Window *window) {
    return SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
}

SDL_Texture *createEICAS1LogicTexture(SDL_Renderer *renderer) {
    return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, EICAS1_WIDTH, EICAS1_HEIGHT);
}

void destroyEICAS1(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *logic_texture) {
    if (eicas1_font12 != NULL) TTF_CloseFont(eicas1_font12);
    if (eicas1_font16 != NULL) TTF_CloseFont(eicas1_font16);
    if (eicas1_font20 != NULL) TTF_CloseFont(eicas1_font20);
    if (eicas1_font28 != NULL) TTF_CloseFont(eicas1_font28);
    if (logic_texture != NULL) SDL_DestroyTexture(logic_texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void drawEICAS1Background(SDL_Renderer *renderer) {
    boxRGBA(renderer, 0, 0, EICAS1_WIDTH, EICAS1_HEIGHT, 0, 0, 0, 255);
    roundedRectangleRGBA(renderer, 26, 18, 746, 695, 18, 120, 130, 140, 255);
    render_text(renderer, 46, 34, "TAT", eicas1_font16, EICAS1_WHITE);

    {
        char tat[16];
        snprintf(tat, sizeof(tat), "%4.1f C", eicas1_tat);
        render_text(renderer, 90, 32, tat, eicas1_font20, EICAS1_CYAN);
    }
}

void drawEICAS1Primary(SDL_Renderer *renderer) {
    draw_engine_gauge(renderer, 244, 120, eicas1_n1_left, eicas1_egt_left, eicas1_ff_left, "ENG 1");
    draw_engine_gauge(renderer, 528, 120, eicas1_n1_right, eicas1_egt_right, eicas1_ff_right, "ENG 2");
}

void drawEICAS1Fuel(SDL_Renderer *renderer) {
    char value[24];
    float total = eicas1_fuel_center + eicas1_fuel_left + eicas1_fuel_right;
    int center_h = (int)(clamp_float(eicas1_fuel_center / 8000.0f, 0.0f, 1.0f) * 140.0f + 0.5f);
    int left_h = (int)(clamp_float(eicas1_fuel_left / 6000.0f, 0.0f, 1.0f) * 140.0f + 0.5f);
    int right_h = (int)(clamp_float(eicas1_fuel_right / 6000.0f, 0.0f, 1.0f) * 140.0f + 0.5f);

    render_text_centered(renderer, 386, 436, "FUEL", eicas1_font20, EICAS1_WHITE);
    boxRGBA(renderer, 186, 520, 234, 660, 28, 30, 34, 255);
    boxRGBA(renderer, 362, 500, 410, 660, 28, 30, 34, 255);
    boxRGBA(renderer, 538, 520, 586, 660, 28, 30, 34, 255);
    boxRGBA(renderer, 187, 660 - left_h, 233, 659, 80, 236, 124, 255);
    boxRGBA(renderer, 363, 660 - center_h, 409, 659, 80, 236, 124, 255);
    boxRGBA(renderer, 539, 660 - right_h, 585, 659, 80, 236, 124, 255);
    rectangleRGBA(renderer, 186, 520, 234, 660, 180, 180, 180, 255);
    rectangleRGBA(renderer, 362, 500, 410, 660, 180, 180, 180, 255);
    rectangleRGBA(renderer, 538, 520, 586, 660, 180, 180, 180, 255);
    render_text_centered(renderer, 210, 666, "L", eicas1_font16, EICAS1_WHITE);
    render_text_centered(renderer, 386, 666, "CTR", eicas1_font16, EICAS1_WHITE);
    render_text_centered(renderer, 562, 666, "R", eicas1_font16, EICAS1_WHITE);

    snprintf(value, sizeof(value), "%5.0f", eicas1_fuel_left);
    render_text_centered(renderer, 210, 488, value, eicas1_font16, EICAS1_GREEN);
    snprintf(value, sizeof(value), "%5.0f", eicas1_fuel_center);
    render_text_centered(renderer, 386, 468, value, eicas1_font16, EICAS1_GREEN);
    snprintf(value, sizeof(value), "%5.0f", eicas1_fuel_right);
    render_text_centered(renderer, 562, 488, value, eicas1_font16, EICAS1_GREEN);
    snprintf(value, sizeof(value), "TOTAL %6.0f", total);
    render_text_centered(renderer, 386, 690, value, eicas1_font16, EICAS1_CYAN);
}

void drawEICAS1Status(SDL_Renderer *renderer) {
    const char *source = "STATIC";
    const char *status = eicas1_xplane_connected ? "UDP OK" : "SIM";

    if (eicas1_data_source == EICAS1_SOURCE_FILE) source = "FILE";
    else if (eicas1_data_source == EICAS1_SOURCE_XPLANE) source = "XPLANE";
    render_text(renderer, 628, 34, source, eicas1_font16, EICAS1_GREEN);
    render_text(renderer, 628, 54, status, eicas1_font12, eicas1_xplane_connected ? EICAS1_GREEN : EICAS1_AMBER);
}
