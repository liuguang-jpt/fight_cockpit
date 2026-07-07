#include "map_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TTF_Font *g_map_font14 = NULL;
TTF_Font *g_map_font18 = NULL;
TTF_Font *g_map_font26 = NULL;
SDL_Texture *g_map_texture = NULL;
SDL_Texture *g_plane_texture = NULL;
SDL_Texture *g_zoom_in_texture = NULL;
SDL_Texture *g_zoom_out_texture = NULL;
MapLayout g_map_layout;

static unsigned int g_map_texture_version = 0;
static int g_map_window_width = MAP_WINDOW_WIDTH;
static int g_map_window_height = MAP_WINDOW_HEIGHT;

static const SDL_Color MAP_BG = {11, 16, 22, 255};
static const SDL_Color MAP_PANEL = {18, 27, 36, 255};
static const SDL_Color MAP_STROKE = {84, 112, 138, 255};
static const SDL_Color MAP_TEXT = {241, 244, 248, 255};
static const SDL_Color MAP_MUTED = {153, 177, 196, 255};
static const SDL_Color MAP_CYAN = {69, 203, 255, 255};
static const SDL_Color MAP_GREEN = {97, 235, 146, 255};
static const SDL_Color MAP_AMBER = {255, 190, 89, 255};
static const SDL_Color MAP_MAGENTA = {232, 83, 208, 255};

static int point_in_rect(int x, int y, const SDL_Rect *rect) {
    if (rect == NULL) return 0;
    return x >= rect->x && x < rect->x + rect->w && y >= rect->y && y < rect->y + rect->h;
}

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

static void render_text_right(SDL_Renderer *renderer, int right_x, int y, const char *text, TTF_Font *font, SDL_Color color) {
    int text_w = 0;
    int text_h = 0;

    if (renderer == NULL || font == NULL || text == NULL || *text == '\0') return;
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) == -1) return;
    render_text(renderer, right_x - text_w, y, text, font, color);
}

static void draw_panel(SDL_Renderer *renderer, const SDL_Rect *rect, const char *title, SDL_Color accent) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, MAP_PANEL.r, MAP_PANEL.g, MAP_PANEL.b, 230);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, MAP_STROKE.r, MAP_STROKE.g, MAP_STROKE.b, 255);
    SDL_RenderDrawRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 255);
    SDL_Rect stripe = {rect->x, rect->y, rect->w, 4};
    SDL_RenderFillRect(renderer, &stripe);
    render_text(renderer, rect->x + 16, rect->y + 14, title, g_map_font18, MAP_TEXT);
}

static double clamp_double(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void geo_to_map_screen(const MapSnapshot *snapshot, double latitude, double longitude, int *out_x, int *out_y) {
    double scale = 256.0 * pow(2.0, snapshot->zoom_level);
    double center_x = (snapshot->center_longitude + 180.0) / 360.0 * scale;
    double center_y = (1.0 - log(tan(snapshot->center_latitude * M_PI / 180.0) + 1.0 / cos(snapshot->center_latitude * M_PI / 180.0)) / M_PI) * 0.5 * scale;
    double point_x = (longitude + 180.0) / 360.0 * scale;
    double point_y = (1.0 - log(tan(latitude * M_PI / 180.0) + 1.0 / cos(latitude * M_PI / 180.0)) / M_PI) * 0.5 * scale;
    double pixel_x = (point_x - center_x) + MAP_TEXTURE_WIDTH * 0.5;
    double pixel_y = (point_y - center_y) + MAP_TEXTURE_HEIGHT * 0.5;
    double normalized_x = pixel_x / MAP_TEXTURE_WIDTH;
    double normalized_y = pixel_y / MAP_TEXTURE_HEIGHT;

    normalized_x = clamp_double(normalized_x, -0.25, 1.25);
    normalized_y = clamp_double(normalized_y, -0.25, 1.25);
    *out_x = g_map_layout.map_rect.x + (int)lround(normalized_x * g_map_layout.map_rect.w);
    *out_y = g_map_layout.map_rect.y + (int)lround(normalized_y * g_map_layout.map_rect.h);
}

static void draw_track(SDL_Renderer *renderer, const MapSnapshot *snapshot) {
    int i;

    if (snapshot == NULL || snapshot->track_count < 2) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (i = 1; i < snapshot->track_count; ++i) {
        int x1 = 0;
        int y1 = 0;
        int x2 = 0;
        int y2 = 0;
        Uint8 alpha = (Uint8)(60 + (195 * i) / snapshot->track_count);

        geo_to_map_screen(snapshot, snapshot->track[i - 1].latitude, snapshot->track[i - 1].longitude, &x1, &y1);
        geo_to_map_screen(snapshot, snapshot->track[i].latitude, snapshot->track[i].longitude, &x2, &y2);
        SDL_SetRenderDrawColor(renderer, MAP_CYAN.r, MAP_CYAN.g, MAP_CYAN.b, alpha);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
}

static void draw_plane(SDL_Renderer *renderer, const MapSnapshot *snapshot) {
    SDL_Rect plane_rect = {0, 0, 40, 40};
    int center_x = 0;
    int center_y = 0;
    SDL_Point center = {20, 20};

    if (snapshot == NULL || g_plane_texture == NULL) return;
    geo_to_map_screen(snapshot, snapshot->aircraft_latitude, snapshot->aircraft_longitude, &center_x, &center_y);
    plane_rect.x = center_x - plane_rect.w / 2;
    plane_rect.y = center_y - plane_rect.h / 2;
    SDL_RenderCopyEx(renderer, g_plane_texture, NULL, &plane_rect, snapshot->aircraft_heading_deg, &center, SDL_FLIP_NONE);
}

static void draw_city_panel(SDL_Renderer *renderer, const MapSnapshot *snapshot) {
    char line[160];
    SDL_Rect rect = g_map_layout.city_rect;

    draw_panel(renderer, &rect, "Location", MAP_CYAN);
    snprintf(line, sizeof(line), "%s / %s", snapshot->city, snapshot->district);
    render_text(renderer, rect.x + 16, rect.y + 54, line, g_map_font26, MAP_TEXT);
    render_text(renderer, rect.x + 16, rect.y + 94, snapshot->address, g_map_font14, MAP_MUTED);
    render_text(renderer, rect.x + 16, rect.y + 128, "Position Source", g_map_font14, MAP_MUTED);
    render_text_right(renderer, rect.x + rect.w - 16, rect.y + 128, snapshot->position_source, g_map_font14, MAP_GREEN);
    render_text(renderer, rect.x + 16, rect.y + 154, "Map Source", g_map_font14, MAP_MUTED);
    render_text_right(renderer, rect.x + rect.w - 16, rect.y + 154, snapshot->map_source, g_map_font14, MAP_GREEN);
}

static void draw_weather_panel(SDL_Renderer *renderer, const MapSnapshot *snapshot) {
    SDL_Rect rect = g_map_layout.weather_rect;

    draw_panel(renderer, &rect, "Weather", MAP_AMBER);
    render_text(renderer, rect.x + 16, rect.y + 54, snapshot->weather_text, g_map_font26, MAP_TEXT);
    render_text(renderer, rect.x + 16, rect.y + 92, "Temperature", g_map_font14, MAP_MUTED);
    render_text_right(renderer, rect.x + rect.w - 16, rect.y + 92, snapshot->temperature_c, g_map_font18, MAP_TEXT);
    render_text(renderer, rect.x + 16, rect.y + 120, "Humidity", g_map_font14, MAP_MUTED);
    render_text_right(renderer, rect.x + rect.w - 16, rect.y + 120, snapshot->humidity, g_map_font18, MAP_TEXT);
    render_text(renderer, rect.x + 16, rect.y + 148, "Wind", g_map_font14, MAP_MUTED);
    render_text_right(renderer, rect.x + rect.w - 16, rect.y + 148, snapshot->wind_direction, g_map_font18, MAP_TEXT);
    render_text(renderer, rect.x + 16, rect.y + 176, "Power", g_map_font14, MAP_MUTED);
    render_text_right(renderer, rect.x + rect.w - 16, rect.y + 176, snapshot->wind_power, g_map_font18, MAP_TEXT);
    render_text(renderer, rect.x + 16, rect.y + 206, snapshot->report_time, g_map_font14, MAP_MUTED);
}

static void draw_flight_panel(SDL_Renderer *renderer, const MapSnapshot *snapshot) {
    char line[64];
    SDL_Rect rect = g_map_layout.flight_rect;

    draw_panel(renderer, &rect, "Flight Track", MAP_MAGENTA);
    snprintf(line, sizeof(line), "%.4f / %.4f", snapshot->aircraft_latitude, snapshot->aircraft_longitude);
    render_text(renderer, rect.x + 16, rect.y + 54, line, g_map_font18, MAP_TEXT);
    snprintf(line, sizeof(line), "HDG %.0f", snapshot->aircraft_heading_deg);
    render_text(renderer, rect.x + 16, rect.y + 86, line, g_map_font18, MAP_GREEN);
    snprintf(line, sizeof(line), "GS %.0f KT", snapshot->groundspeed_kts);
    render_text(renderer, rect.x + 16, rect.y + 118, line, g_map_font18, MAP_GREEN);
    snprintf(line, sizeof(line), "ZOOM %d", snapshot->zoom_level);
    render_text(renderer, rect.x + 16, rect.y + 150, line, g_map_font18, MAP_CYAN);
    snprintf(line, sizeof(line), "TRACK PTS %d", snapshot->track_count);
    render_text(renderer, rect.x + 16, rect.y + 182, line, g_map_font18, MAP_TEXT);
    render_text(renderer, rect.x + 16, rect.y + rect.h - 32, snapshot->status_line, g_map_font14, MAP_MUTED);
}

static void draw_map_panel(SDL_Renderer *renderer, const MapSnapshot *snapshot) {
    SDL_Rect rect = g_map_layout.map_rect;
    SDL_Rect header = {rect.x, rect.y, rect.w, 4};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, MAP_PANEL.r, MAP_PANEL.g, MAP_PANEL.b, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, MAP_STROKE.r, MAP_STROKE.g, MAP_STROKE.b, 255);
    SDL_RenderDrawRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, MAP_GREEN.r, MAP_GREEN.g, MAP_GREEN.b, 255);
    SDL_RenderFillRect(renderer, &header);
    if (g_map_texture != NULL) SDL_RenderCopy(renderer, g_map_texture, NULL, &rect);
    draw_track(renderer, snapshot);
    draw_plane(renderer, snapshot);
    render_text(renderer, rect.x + 18, rect.y + 16, "Amap Static Map / Reverse Geocode / Weather", g_map_font18, MAP_TEXT);
    render_text(renderer, rect.x + 18, rect.y + 42, "Mouse wheel or buttons change zoom", g_map_font14, MAP_MUTED);
    SDL_RenderCopy(renderer, g_zoom_in_texture, NULL, &g_map_layout.zoom_in_rect);
    SDL_RenderCopy(renderer, g_zoom_out_texture, NULL, &g_map_layout.zoom_out_rect);
}

int initMap(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) return -1;
    if (TTF_Init() == -1) return -1;
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG) return -1;

    g_map_font14 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 14);
    g_map_font18 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 18);
    g_map_font26 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 26);
    if (g_map_font14 == NULL || g_map_font18 == NULL || g_map_font26 == NULL) return -1;
    return 0;
}

SDL_Window *createMapWindow(void) {
    return SDL_CreateWindow("MAP", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, MAP_WINDOW_WIDTH, MAP_WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
}

SDL_Renderer *createMapRenderer(SDL_Window *window) {
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (renderer == NULL) return NULL;
    g_plane_texture = IMG_LoadTexture(renderer, "assets/assets/assets/plane.png");
    g_zoom_in_texture = IMG_LoadTexture(renderer, "assets/assets/assets/add.png");
    g_zoom_out_texture = IMG_LoadTexture(renderer, "assets/assets/assets/sub.png");
    return renderer;
}

void destroyMap(SDL_Window *window, SDL_Renderer *renderer) {
    if (g_map_texture != NULL) SDL_DestroyTexture(g_map_texture);
    if (g_plane_texture != NULL) SDL_DestroyTexture(g_plane_texture);
    if (g_zoom_in_texture != NULL) SDL_DestroyTexture(g_zoom_in_texture);
    if (g_zoom_out_texture != NULL) SDL_DestroyTexture(g_zoom_out_texture);
    if (g_map_font14 != NULL) TTF_CloseFont(g_map_font14);
    if (g_map_font18 != NULL) TTF_CloseFont(g_map_font18);
    if (g_map_font26 != NULL) TTF_CloseFont(g_map_font26);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void initMapLayout(int width, int height) {
    int pad = width / 60;
    int side_w;
    int top_y;
    int stack_h;

    if (pad < 16) pad = 16;
    g_map_window_width = width;
    g_map_window_height = height;
    side_w = width / 4;
    if (side_w < 300) side_w = 300;
    if (side_w > 360) side_w = 360;

    g_map_layout.title_rect = (SDL_Rect){pad, pad, width - pad * 2, 58};
    top_y = g_map_layout.title_rect.y + g_map_layout.title_rect.h + pad;
    stack_h = height - top_y - pad;

    g_map_layout.city_rect = (SDL_Rect){pad, top_y, side_w, stack_h / 4};
    g_map_layout.weather_rect = (SDL_Rect){pad, g_map_layout.city_rect.y + g_map_layout.city_rect.h + pad, side_w, stack_h / 3};
    g_map_layout.flight_rect = (SDL_Rect){
        pad,
        g_map_layout.weather_rect.y + g_map_layout.weather_rect.h + pad,
        side_w,
        height - (g_map_layout.weather_rect.y + g_map_layout.weather_rect.h + pad) - pad
    };
    g_map_layout.map_rect = (SDL_Rect){pad * 2 + side_w, top_y, width - side_w - pad * 3, height - top_y - pad};
    g_map_layout.zoom_in_rect = (SDL_Rect){g_map_layout.map_rect.x + g_map_layout.map_rect.w - 56, g_map_layout.map_rect.y + 16, 40, 40};
    g_map_layout.zoom_out_rect = (SDL_Rect){g_map_layout.map_rect.x + g_map_layout.map_rect.w - 56, g_map_layout.map_rect.y + 64, 40, 40};
}

void map_refresh_dynamic_texture(SDL_Renderer *renderer) {
    unsigned char *bytes = NULL;
    size_t size = 0;
    unsigned int version = 0;
    SDL_RWops *rw = NULL;
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;

    if (renderer == NULL) return;
    if (!map_copy_latest_image(&bytes, &size, &version)) return;
    if (version == g_map_texture_version) {
        free(bytes);
        return;
    }

    rw = SDL_RWFromConstMem(bytes, (int)size);
    if (rw == NULL) {
        free(bytes);
        return;
    }
    surface = IMG_Load_RW(rw, 1);
    free(bytes);
    if (surface == NULL) return;

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture == NULL) return;

    if (g_map_texture != NULL) SDL_DestroyTexture(g_map_texture);
    g_map_texture = texture;
    g_map_texture_version = version;
}

void drawMapScreen(SDL_Renderer *renderer, const MapSnapshot *snapshot) {
    SDL_SetRenderDrawColor(renderer, MAP_BG.r, MAP_BG.g, MAP_BG.b, MAP_BG.a);
    SDL_RenderClear(renderer);
    draw_panel(renderer, &g_map_layout.title_rect, "MAP Module", MAP_GREEN);
    render_text(renderer, g_map_layout.title_rect.x + 16, g_map_layout.title_rect.y + 30, "City info, weather, static map texture and flight trail", g_map_font14, MAP_MUTED);
    render_text_right(renderer, g_map_layout.title_rect.x + g_map_layout.title_rect.w - 16, g_map_layout.title_rect.y + 18, snapshot->position_source, g_map_font18, MAP_GREEN);
    draw_city_panel(renderer, snapshot);
    draw_weather_panel(renderer, snapshot);
    draw_flight_panel(renderer, snapshot);
    draw_map_panel(renderer, snapshot);
}

int map_handle_click(int x, int y) {
    if (point_in_rect(x, y, &g_map_layout.zoom_in_rect)) return 1;
    if (point_in_rect(x, y, &g_map_layout.zoom_out_rect)) return -1;
    return 0;
}
