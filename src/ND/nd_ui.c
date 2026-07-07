#include "nd_ui.h"

#include "nd_data.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ND_WIDTH 772
#define ND_HEIGHT 721
#define ND_PI 3.14159265f

TTF_Font *nd_font10 = NULL, *nd_font12 = NULL, *nd_font16 = NULL, *nd_font20 = NULL, *nd_font28 = NULL;
int nd_window_width = ND_WIDTH, nd_window_height = ND_HEIGHT;
SDL_Texture *nd_logic_texture = NULL;
SDL_Rect nd_render_rect = {0, 0, ND_WIDTH, ND_HEIGHT};

static const SDL_Color ND_WHITE = {245, 245, 245, 255};
static const SDL_Color ND_GREEN = {80, 236, 124, 255};
static const SDL_Color ND_AMBER = {255, 188, 64, 255};
static const SDL_Color ND_CYAN = {77, 219, 255, 255};
static const SDL_Color ND_MAGENTA = {231, 48, 223, 255};

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
    int text_w = 0, text_h = 0;
    if (font == NULL || text == NULL || *text == '\0') return;
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) == -1) return;
    render_text(renderer, center_x - text_w / 2, y, text, font, color);
}

static int normalize_heading(int value) {
    int heading = value % 360;
    if (heading < 0) heading += 360;
    return heading;
}

static void draw_panel(SDL_Renderer *renderer, int x, int y, int w, int h, int radius) {
    roundedBoxRGBA(renderer, x, y, x + w, y + h, radius, 7, 11, 14, 240);
    roundedRectangleRGBA(renderer, x, y, x + w, y + h, radius, 120, 130, 140, 255);
}

static void draw_heading_arc(SDL_Renderer *renderer, int center_x, int center_y, int radius, int heading_deg) {
    int rel;

    arcRGBA(renderer, center_x, center_y, radius, 210, 330, 175, 185, 195, 255);
    arcRGBA(renderer, center_x, center_y, radius - 1, 210, 330, 175, 185, 195, 255);

    for (rel = -60; rel <= 60; rel += 5) {
        int absolute_heading = normalize_heading(heading_deg + rel);
        int tick_len = absolute_heading % 30 == 0 ? 20 : 12;
        float radians = (rel - 90.0f) * ND_PI / 180.0f;
        int x1 = center_x + (int)(cosf(radians) * radius + 0.5f);
        int y1 = center_y + (int)(sinf(radians) * radius + 0.5f);
        int x2 = center_x + (int)(cosf(radians) * (radius - tick_len) + 0.5f);
        int y2 = center_y + (int)(sinf(radians) * (radius - tick_len) + 0.5f);

        lineRGBA(renderer, x1, y1, x2, y2, 255, 255, 255, 255);
        if (absolute_heading % 30 == 0) {
            char label[8];
            int tx = center_x + (int)(cosf(radians) * (radius - 38) + 0.5f);
            int ty = center_y + (int)(sinf(radians) * (radius - 38) + 0.5f);

            snprintf(label, sizeof(label), "%02d", absolute_heading / 10);
            render_text_centered(renderer, tx, ty - 9, label, nd_font12, ND_WHITE);
        }
    }

    filledTrigonRGBA(renderer, center_x - 12, center_y - radius - 3, center_x + 12, center_y - radius - 3, center_x, center_y - radius + 13, 255, 255, 255, 255);
}

static void draw_waypoint(SDL_Renderer *renderer, int center_x, int center_y, int radius, int heading_deg, const NDRoutePoint *point) {
    float relative_bearing, radians, distance_ratio;
    int x, y;
    Sint16 px[4], py[4];

    if (point == NULL || !point->is_active) return;

    /* Bearings are stored in world coordinates, so convert them into aircraft-relative screen angles first. */
    relative_bearing = point->bearing_deg - (float)heading_deg;
    while (relative_bearing > 180.0f) relative_bearing -= 360.0f;
    while (relative_bearing < -180.0f) relative_bearing += 360.0f;

    radians = (relative_bearing - 90.0f) * ND_PI / 180.0f;
    distance_ratio = point->distance_nm / 80.0f;
    if (distance_ratio > 1.0f) distance_ratio = 1.0f;

    x = center_x + (int)(cosf(radians) * radius * distance_ratio + 0.5f);
    y = center_y + (int)(sinf(radians) * radius * distance_ratio + 0.5f);
    px[0] = (Sint16)x; py[0] = (Sint16)(y - 7);
    px[1] = (Sint16)(x + 7); py[1] = (Sint16)y;
    px[2] = (Sint16)x; py[2] = (Sint16)(y + 7);
    px[3] = (Sint16)(x - 7); py[3] = (Sint16)y;

    polygonRGBA(renderer, px, py, 4, 77, 219, 255, 255);
    render_text_centered(renderer, x, y + 10, point->name, nd_font10, ND_CYAN);
}

int initND(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) {
        printf("ND SDL init failed: %s\n", SDL_GetError());
        return -1;
    }
    if (TTF_Init() == -1) {
        printf("ND TTF init failed: %s\n", TTF_GetError());
        return -1;
    }

    nd_font10 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 10);
    nd_font12 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 12);
    nd_font16 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 16);
    nd_font20 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 20);
    nd_font28 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 28);
    if (nd_font10 == NULL || nd_font12 == NULL || nd_font16 == NULL || nd_font20 == NULL || nd_font28 == NULL) {
        printf("ND font load failed: %s\n", TTF_GetError());
        return -1;
    }
    if (IMG_Init(IMG_INIT_PNG) == -1) {
        printf("ND image init failed: %s\n", IMG_GetError());
        return -1;
    }
    return 0;
}

SDL_Window *createNDWindow(void) {
    SDL_Window *window = SDL_CreateWindow(
        "ND", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        nd_window_width, nd_window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (window == NULL) printf("ND window creation failed: %s\n", SDL_GetError());
    return window;
}

SDL_Renderer *createNDRenderer(SDL_Window *window) {
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) printf("ND renderer creation failed: %s\n", SDL_GetError());
    return renderer;
}

SDL_Texture *createNDLogicTexture(SDL_Renderer *renderer) {
    SDL_Texture *target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, ND_WIDTH, ND_HEIGHT);
    if (target == NULL) printf("ND logic texture creation failed: %s\n", SDL_GetError());
    return target;
}

void destroyND(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *logic_texture) {
    if (nd_font10 != NULL) TTF_CloseFont(nd_font10);
    if (nd_font12 != NULL) TTF_CloseFont(nd_font12);
    if (nd_font16 != NULL) TTF_CloseFont(nd_font16);
    if (nd_font20 != NULL) TTF_CloseFont(nd_font20);
    if (nd_font28 != NULL) TTF_CloseFont(nd_font28);
    nd_font10 = nd_font12 = nd_font16 = nd_font20 = nd_font28 = NULL;

    if (logic_texture != NULL) SDL_DestroyTexture(logic_texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void initNDLayout(void) {
}

void drawNDBackground(SDL_Renderer *renderer) {
    int i;
    boxRGBA(renderer, 0, 0, ND_WIDTH, ND_HEIGHT, 0, 0, 0, 255);
    for (i = 0; i < ND_HEIGHT; i += 24) aalineRGBA(renderer, 0, i, ND_WIDTH, i, 12, 18, 24, 140);
}

void drawNDMapMode(SDL_Renderer *renderer) {
    const int center_x = ND_WIDTH / 2, center_y = 458, range_radius = 235;
    int i, track_diff = normalize_heading(nd_current_track - nd_current_heading);

    draw_panel(renderer, 18, 16, 736, 90, 14);
    draw_panel(renderer, 50, 104, 672, 584, 18);

    render_text(renderer, 38, 28, "MODE", nd_font12, ND_WHITE);
    render_text(renderer, 38, 47, "MAP", nd_font20, ND_GREEN);
    render_text(renderer, 146, 28, "RANGE", nd_font12, ND_WHITE);
    render_text(renderer, 146, 47, "80 NM", nd_font20, ND_GREEN);
    render_text(renderer, 288, 28, "GS", nd_font12, ND_WHITE);
    render_text(renderer, 288, 47, "260", nd_font20, ND_GREEN);
    render_text(renderer, 378, 28, "TAS", nd_font12, ND_WHITE);
    render_text(renderer, 378, 47, "260", nd_font20, ND_GREEN);
    render_text(renderer, 490, 28, "WIND", nd_font12, ND_WHITE);

    {
        char value[24];
        snprintf(value, sizeof(value), "%03d/%02d", nd_wind_direction, nd_wind_speed);
        render_text(renderer, 490, 47, value, nd_font20, ND_CYAN);
    }

    draw_heading_arc(renderer, center_x, center_y, range_radius, nd_current_heading);
    circleRGBA(renderer, center_x, center_y, range_radius, 58, 64, 70, 220);
    circleRGBA(renderer, center_x, center_y, range_radius / 2, 45, 52, 58, 220);
    aalineRGBA(renderer, center_x, 132, center_x, 636, 45, 52, 58, 220);
    aalineRGBA(renderer, 150, center_y, 622, center_y, 45, 52, 58, 220);

    for (i = 0; i < ND_ROUTE_POINT_COUNT; ++i)
        draw_waypoint(renderer, center_x, center_y, range_radius, nd_current_heading, &nd_route_points[i]);

    for (i = 0; i < ND_ROUTE_POINT_COUNT - 1; ++i) {
        float bearing_a, bearing_b, radians_a, radians_b, dist_a, dist_b;
        int ax, ay, bx, by;

        if (!nd_route_points[i].is_active || !nd_route_points[i + 1].is_active) continue;
        bearing_a = nd_route_points[i].bearing_deg - (float)nd_current_heading;
        bearing_b = nd_route_points[i + 1].bearing_deg - (float)nd_current_heading;
        while (bearing_a > 180.0f) bearing_a -= 360.0f;
        while (bearing_a < -180.0f) bearing_a += 360.0f;
        while (bearing_b > 180.0f) bearing_b -= 360.0f;
        while (bearing_b < -180.0f) bearing_b += 360.0f;

        /* Route lines use the same bearing/range projection as waypoint symbols, so both stay aligned while rotating. */
        radians_a = (bearing_a - 90.0f) * ND_PI / 180.0f;
        radians_b = (bearing_b - 90.0f) * ND_PI / 180.0f;
        dist_a = nd_route_points[i].distance_nm / 80.0f;
        dist_b = nd_route_points[i + 1].distance_nm / 80.0f;
        if (dist_a > 1.0f) dist_a = 1.0f;
        if (dist_b > 1.0f) dist_b = 1.0f;

        ax = center_x + (int)(cosf(radians_a) * range_radius * dist_a + 0.5f);
        ay = center_y + (int)(sinf(radians_a) * range_radius * dist_a + 0.5f);
        bx = center_x + (int)(cosf(radians_b) * range_radius * dist_b + 0.5f);
        by = center_y + (int)(sinf(radians_b) * range_radius * dist_b + 0.5f);
        aalineRGBA(renderer, ax, ay, bx, by, 231, 48, 223, 255);
    }

    {
        float radians = ((float)track_diff - 90.0f) * ND_PI / 180.0f;
        int tx = center_x + (int)(cosf(radians) * 46.0f + 0.5f);
        int ty = center_y + (int)(sinf(radians) * 46.0f + 0.5f);
        Sint16 px[] = {(Sint16)tx, (Sint16)(tx + 8), (Sint16)tx, (Sint16)(tx - 8)};
        Sint16 py[] = {(Sint16)(ty - 8), (Sint16)ty, (Sint16)(ty + 8), (Sint16)ty};

        polygonRGBA(renderer, px, py, 4, 255, 255, 255, 255);
    }

    filledTrigonRGBA(renderer, center_x - 14, center_y + 130, center_x + 14, center_y + 130, center_x, center_y + 96, 255, 255, 255, 255);
    aalineRGBA(renderer, center_x, center_y - 32, center_x, center_y + 104, 255, 255, 255, 255);
    aalineRGBA(renderer, center_x - 34, center_y + 84, center_x + 34, center_y + 84, 255, 255, 255, 255);

    render_text_centered(renderer, center_x - range_radius + 18, center_y - 10, "40", nd_font12, ND_WHITE);
    render_text_centered(renderer, center_x - range_radius + 18, center_y - range_radius / 2 - 8, "80", nd_font12, ND_WHITE);
    render_text(renderer, 72, 630, "WXR", nd_font16, ND_AMBER);
    render_text(renderer, 132, 630, "TFC", nd_font16, ND_WHITE);
    render_text(renderer, 194, 630, "ARPT", nd_font16, ND_CYAN);
    render_text(renderer, 276, 630, "WPT", nd_font16, ND_MAGENTA);
}

void drawNDStatus(SDL_Renderer *renderer) {
    const char *source = "STATIC";
    const char *status = nd_xplane_connected ? "UDP OK" : "SIM";
    const char *route_status = nd_fmc_link_active ? "FMC LINK" : "LOCAL RTE";

    if (nd_data_source == ND_SOURCE_FILE) source = "FILE";
    else if (nd_data_source == ND_SOURCE_XPLANE) source = "XPLANE";

    render_text(renderer, 632, 28, "SOURCE", nd_font12, ND_WHITE);
    render_text(renderer, 632, 47, source, nd_font16, ND_GREEN);
    render_text(renderer, 632, 69, status, nd_font12, nd_xplane_connected ? ND_GREEN : ND_AMBER);
    render_text(renderer, 632, 90, route_status, nd_font12, nd_fmc_link_active ? ND_MAGENTA : ND_WHITE);
}
