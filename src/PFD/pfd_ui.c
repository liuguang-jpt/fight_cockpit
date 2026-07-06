#include "pfd_ui.h"

#include "pfd_data.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PFD_WIDTH 772
#define PFD_HEIGHT 721
#define PI_F 3.14159265f

int window_width = PFD_WIDTH;
int window_height = PFD_HEIGHT;
TTF_Font *font8 = NULL;
TTF_Font *font12 = NULL;
TTF_Font *font16 = NULL;
TTF_Font *font20 = NULL;
TTF_Font *font24 = NULL;
SDL_Rect g_render_rect = {0, 0, PFD_WIDTH, PFD_HEIGHT};
SDL_Texture *logic_texture = NULL;

static const SDL_Color COLOR_WHITE = {245, 245, 245, 255};
static const SDL_Color COLOR_MAGENTA = {231, 48, 223, 255};
static const SDL_Color COLOR_GREEN = {94, 223, 132, 255};
static const SDL_Color COLOR_AMBER = {255, 188, 64, 255};

int initPFD(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) {
        printf("PFD的SDL初始化失败: %s\n", SDL_GetError());
        return -1;
    }
    if (TTF_Init() == -1) {
        printf("PFD的SDL字体初始化失败: %s\n", TTF_GetError());
        return -1;
    }

    font8 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 8);
    font12 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 12);
    font16 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 16);
    font20 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 20);
    font24 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 24);
    if (font8 == NULL || font12 == NULL || font16 == NULL || font20 == NULL || font24 == NULL) {
        printf("PFD字体加载失败: %s\n", TTF_GetError());
        return -1;
    }

    if (IMG_Init(IMG_INIT_PNG) == -1) {
        printf("PFD的SDL图片初始化失败: %s\n", IMG_GetError());
        return -1;
    }
    return 0;
}

SDL_Window *createPFDWindow(void) {
    SDL_Window *window = SDL_CreateWindow("PFD",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_width,
        window_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (window == NULL) {
        printf("PFD窗口创建失败: %s\n", SDL_GetError());
    }
    return window;
}

SDL_Renderer *createPFDRenderer(SDL_Window *window) {
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (renderer == NULL) {
        printf("PFD渲染器创建失败: %s\n", SDL_GetError());
    }
    return renderer;
}

SDL_Texture *createLogicTexture(SDL_Renderer *renderer) {
    SDL_Texture *target = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        PFD_WIDTH,
        PFD_HEIGHT);

    if (target == NULL) {
        printf("逻辑纹理创建失败: %s\n", SDL_GetError());
    }
    return target;
}

void destroyPFD(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *target) {
    if (font8 != NULL) TTF_CloseFont(font8);
    if (font12 != NULL) TTF_CloseFont(font12);
    if (font16 != NULL) TTF_CloseFont(font16);
    if (font20 != NULL) TTF_CloseFont(font20);
    if (font24 != NULL) TTF_CloseFont(font24);

    font8 = NULL;
    font12 = NULL;
    font16 = NULL;
    font20 = NULL;
    font24 = NULL;

    if (target != NULL) SDL_DestroyTexture(target);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int normalize_heading(int heading) {
    int value = heading % 360;
    if (value < 0) value += 360;
    return value;
}

static void render_ttf_text(SDL_Renderer *renderer, int x, int y,
        const char *text, TTF_Font *font, SDL_Color color) {
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;
    SDL_Rect dest;

    if (renderer == NULL || font == NULL || text == NULL || *text == '\0') {
        return;
    }

    surface = TTF_RenderUTF8_Solid(font, text, color);
    if (surface == NULL) {
        return;
    }

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

static void render_ttf_text_centered(SDL_Renderer *renderer, int center_x, int y,
        const char *text, TTF_Font *font, SDL_Color color) {
    int text_w = 0;
    int text_h = 0;

    if (font == NULL || text == NULL || *text == '\0') {
        return;
    }
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) == -1) {
        return;
    }
    render_ttf_text(renderer, center_x - text_w / 2, y, text, font, color);
}

static void render_ttf_text_right(SDL_Renderer *renderer, int right, int y,
        const char *text, TTF_Font *font, SDL_Color color) {
    int text_w = 0;
    int text_h = 0;

    if (font == NULL || text == NULL || *text == '\0') {
        return;
    }
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) == -1) {
        return;
    }
    render_ttf_text(renderer, right - text_w, y, text, font, color);
}

static void rotate_point(float x, float y, float degrees, float *out_x, float *out_y) {
    float radians = degrees * PI_F / 180.0f;
    float c = cosf(radians);
    float s = sinf(radians);

    *out_x = x * c - y * s;
    *out_y = x * s + y * c;
}

static void fill_sky_region(SDL_Renderer *renderer,
        int left, int top, int right, int bottom, float horizon_y, float roll_degrees,
        Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    float slope = tanf(roll_degrees * PI_F / 180.0f);
    const float eps = 1e-6f;
    float center_x = (left + right) * 0.5f;
    int tl = (top < slope * (left - center_x) + horizon_y);
    int tr = (top < slope * (right - center_x) + horizon_y);
    int br = (bottom < slope * (right - center_x) + horizon_y);
    int bl = (bottom < slope * (left - center_x) + horizon_y);
    Sint16 px[12];
    Sint16 py[12];
    int count = 0;
    const int edge_x[4] = {left, right, right, left};
    const int edge_y[4] = {top, top, bottom, bottom};
    const int corner_is_sky[4] = {tl, tr, br, bl};
    int i = 0;

    if (tl && tr && br && bl) {
        boxRGBA(renderer, left, top, right, bottom, r, g, b, a);
        return;
    }
    if (!tl && !tr && !br && !bl) {
        return;
    }

    for (i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        int x1 = edge_x[i];
        int y1 = edge_y[i];
        int x2 = edge_x[next];
        int y2 = edge_y[next];

        if (corner_is_sky[i]) {
            px[count] = (Sint16)x1;
            py[count] = (Sint16)y1;
            ++count;
        }

        if (corner_is_sky[i] != corner_is_sky[next]) {
            float ix = 0.0f;
            float iy = 0.0f;

            if (y1 == y2) {
                if (fabsf(slope) <= eps) {
                    continue;
                }
                ix = center_x + ((float)y1 - horizon_y) / slope;
                iy = (float)y1;
            } else {
                ix = (float)x1;
                iy = slope * (x1 - center_x) + horizon_y;
            }

            if (ix < left) ix = (float)left;
            if (ix > right) ix = (float)right;
            if (iy < top) iy = (float)top;
            if (iy > bottom) iy = (float)bottom;

            px[count] = (Sint16)(ix + 0.5f);
            py[count] = (Sint16)(iy + 0.5f);
            ++count;
        }
    }

    if (count >= 3) {
        filledPolygonRGBA(renderer, px, py, count, r, g, b, a);
    }
}

static int point_is_sky(float x, float y, int left, int right, float horizon_y, float roll_degrees) {
    float slope = tanf(roll_degrees * PI_F / 180.0f);
    float center_x = (left + right) * 0.5f;
    return y < slope * (x - center_x) + horizon_y;
}

static void mask_rounded_corners(SDL_Renderer *renderer,
        int left, int top, int right, int bottom, int radius, float horizon_y, float roll_degrees) {
    int fill_top_left = point_is_sky(left + radius * 0.5f, top + radius * 0.5f, left, right, horizon_y, roll_degrees);
    int fill_top_right = point_is_sky(right - radius * 0.5f, top + radius * 0.5f, left, right, horizon_y, roll_degrees);
    int fill_bottom_right = point_is_sky(right - radius * 0.5f, bottom - radius * 0.5f, left, right, horizon_y, roll_degrees);
    int fill_bottom_left = point_is_sky(left + radius * 0.5f, bottom - radius * 0.5f, left, right, horizon_y, roll_degrees);

    boxRGBA(renderer, left, top, left + radius - 1, top + radius - 1, 0, 0, 0, 255);
    boxRGBA(renderer, right - radius + 1, top, right, top + radius - 1, 0, 0, 0, 255);
    boxRGBA(renderer, left, bottom - radius + 1, left + radius - 1, bottom, 0, 0, 0, 255);
    boxRGBA(renderer, right - radius + 1, bottom - radius + 1, right, bottom, 0, 0, 0, 255);

    filledPieRGBA(renderer, left + radius, top + radius, radius, 180, 270,
        fill_top_left ? 32 : 114,
        fill_top_left ? 116 : 57,
        fill_top_left ? 172 : 5,
        255);
    filledPieRGBA(renderer, right - radius, top + radius, radius, 270, 360,
        fill_top_right ? 32 : 114,
        fill_top_right ? 116 : 57,
        fill_top_right ? 172 : 5,
        255);
    filledPieRGBA(renderer, right - radius, bottom - radius, radius, 0, 90,
        fill_bottom_right ? 32 : 114,
        fill_bottom_right ? 116 : 57,
        fill_bottom_right ? 172 : 5,
        255);
    filledPieRGBA(renderer, left + radius, bottom - radius, radius, 90, 180,
        fill_bottom_left ? 32 : 114,
        fill_bottom_left ? 116 : 57,
        fill_bottom_left ? 172 : 5,
        255);
}

static void draw_panel_frame(SDL_Renderer *renderer, int x, int y, int w, int h, int radius) {
    roundedBoxRGBA(renderer, x, y, x + w, y + h, radius, 10, 13, 15, 230);
    roundedRectangleRGBA(renderer, x, y, x + w, y + h, radius, 130, 137, 144, 255);
}

static void draw_label_value(SDL_Renderer *renderer, int x, int y, const char *label, const char *value,
        SDL_Color label_color, SDL_Color value_color) {
    render_ttf_text(renderer, x, y, label, font12, label_color);
    render_ttf_text(renderer, x, y + 14, value, font16, value_color);
}

static float vsi_value_to_offset(int vsi) {
    static const int marks[] = {0, 500, 1000, 2000, 4000, 6000};
    static const float offsets[] = {0.0f, 28.0f, 56.0f, 100.0f, 150.0f, 185.0f};
    float abs_vsi = (float)(vsi < 0 ? -vsi : vsi);
    int i = 0;

    // VSI 刻度不是线性的：中间 500/1000 更密，外侧 2000/4000/6000 更疏，这里按分段线性插值换算。
    if (abs_vsi >= 6000.0f) {
        return offsets[5];
    }

    for (i = 1; i < 6; ++i) {
        if (abs_vsi <= (float)marks[i]) {
            float ratio = (abs_vsi - (float)marks[i - 1]) / (float)(marks[i] - marks[i - 1]);
            return offsets[i - 1] + ratio * (offsets[i] - offsets[i - 1]);
        }
    }

    return offsets[5];
}

void initPFD_Layout(void) {
}

void drawFlightModeAnnunciator(SDL_Renderer *renderer) {
    const int x = 234;
    const int y = 18;
    const int w = 304;
    const int h = 52;
    const int cell_w = w / 3;
    const char *top_row[3] = {"CWSR", "CMD", "CWSP"};
    const char *bottom_row[3] = {"ROLL", "MAG", "PITCH"};
    int i = 0;

    draw_panel_frame(renderer, x, y, w, h, 10);
    for (i = 1; i < 3; ++i) {
        lineRGBA(renderer, x + i * cell_w, y + 4, x + i * cell_w, y + h - 4, 94, 98, 104, 255);
    }
    lineRGBA(renderer, x + 6, y + 28, x + w - 6, y + 28, 94, 98, 104, 255);

    for (i = 0; i < 3; ++i) {
        int center_x = x + i * cell_w + cell_w / 2;
        SDL_Color top_color = i == 1 ? COLOR_GREEN : COLOR_WHITE;

        render_ttf_text_centered(renderer, center_x, y + 6, top_row[i], font16, top_color);
        render_ttf_text_centered(renderer, center_x, y + 32, bottom_row[i], font12, COLOR_GREEN);
    }
}

void drawAirspeedIndicator(SDL_Renderer *renderer) {
    const int box_x = 46;
    const int box_y = 86;
    const int box_w = 90;
    const int box_h = 505;
    const int center_y = box_y + box_h / 2;
    const int scale_x = box_x + 58;
    const int number_right = scale_x - 12;
    const float px_per_knot = box_h / (float)DISPLAY_RANGE;
    int cur = clamp_int(current_airspeed, AIRSPEED_MIN, AIRSPEED_MAX);
    int tgt = clamp_int(target_airspeed, AIRSPEED_MIN, AIRSPEED_MAX);
    int range_min = ((cur - 50) / 10) * 10;
    int range_max = ((cur + 59) / 10) * 10;
    int speed = 0;

    if (range_min < AIRSPEED_MIN) range_min = AIRSPEED_MIN;
    if (range_max > AIRSPEED_MAX) range_max = AIRSPEED_MAX;

    draw_panel_frame(renderer, box_x, box_y, box_w, box_h, 10);

    {
        char label[16];
        snprintf(label, sizeof(label), "%d", tgt);
        render_ttf_text(renderer, box_x + 11, box_y - 22, label, font20, COLOR_MAGENTA);
    }

    for (speed = range_min; speed <= range_max; speed += 10) {
        int y = center_y - (int)((speed - cur) * px_per_knot + 0.5f);
        int tick_left = (speed % 20 == 0) ? box_x + 25 : box_x + 58;

        if (y < box_y || y > box_y + box_h) continue;
        lineRGBA(renderer, tick_left, y, scale_x + 34, y, 240, 240, 240, 220);

        {
            char text[8];
            snprintf(text, sizeof(text), "%d", speed);
            render_ttf_text_right(renderer, number_right, y - 9, text, font16, COLOR_WHITE);
        }
    }

    {
        int top = center_y - 17;
        int bottom = center_y + 17;
        int left = box_x + 34;
        int right = box_x + 83;
        Sint16 px[] = {
            (Sint16)left, (Sint16)right, (Sint16)(right + 10),
            (Sint16)right, (Sint16)left, (Sint16)left
        };
        Sint16 py[] = {
            (Sint16)top, (Sint16)top, (Sint16)center_y,
            (Sint16)bottom, (Sint16)bottom, (Sint16)top
        };
        char text[8];

        filledPolygonRGBA(renderer, px, py, 6, 0, 0, 0, 255);
        polygonRGBA(renderer, px, py, 6, 255, 255, 255, 255);

        snprintf(text, sizeof(text), "%d", cur);
        render_ttf_text_centered(renderer, (left + right) / 2 + 1, center_y - 11, text, font16, COLOR_WHITE);
    }

    if (tgt >= range_min && tgt <= range_max) {
        int target_y = center_y - (int)((tgt - cur) * px_per_knot + 0.5f);
        Sint16 px[] = {(Sint16)(scale_x + 3), (Sint16)(scale_x + 12), (Sint16)(scale_x + 12), (Sint16)(scale_x + 3)};
        Sint16 py[] = {(Sint16)target_y, (Sint16)(target_y - 7), (Sint16)(target_y + 7), (Sint16)target_y};
        polygonRGBA(renderer, px, py, 4, 231, 48, 223, 255);
    }
}

void drawAltitudeIndicator(SDL_Renderer *renderer) {
    const int box_w = 99;
    const int box_h = 500;
    const int box_x = PFD_WIDTH - 95 - box_w;
    const int box_y = 100;
    const int center_y = box_y + box_h / 2;
    const int scale_x = box_x + 35;
    const int number_x = scale_x + 18;
    const float px_per_foot = box_h / (float)ALTITUDE_DISPLAY_RANGE;
    int cur = clamp_int(current_altitude, ALTITUDE_MIN, ALTITUDE_MAX);
    int tgt = clamp_int(target_altitude, ALTITUDE_MIN, ALTITUDE_MAX);
    int half_range = ALTITUDE_DISPLAY_RANGE / 2;
    int range_min = ((cur - half_range) / ALTITUDE_TICK_INTERVAL) * ALTITUDE_TICK_INTERVAL;
    int range_max = ((cur + half_range + ALTITUDE_TICK_INTERVAL - 1) / ALTITUDE_TICK_INTERVAL) * ALTITUDE_TICK_INTERVAL;
    int altitude = 0;

    range_min = clamp_int(range_min, ALTITUDE_MIN, ALTITUDE_MAX);
    range_max = clamp_int(range_max, ALTITUDE_MIN, ALTITUDE_MAX);

    draw_panel_frame(renderer, box_x, box_y, box_w, box_h, 10);

    {
        char label[16];
        snprintf(label, sizeof(label), "%05d", tgt);
        render_ttf_text(renderer, box_x + 11, box_y - 22, label, font20, COLOR_MAGENTA);
    }

    for (altitude = range_min; altitude <= range_max; altitude += ALTITUDE_TICK_INTERVAL) {
        int y = center_y - (int)((altitude - cur) * px_per_foot + 0.5f);
        int tick_right = (altitude % ALTITUDE_LABEL_INTERVAL == 0) ? scale_x + 36 : scale_x + 22;

        if (y < box_y + 2 || y > box_y + box_h - 2) continue;
        lineRGBA(renderer, box_x + 5, y, tick_right, y, 240, 240, 240, 220);

        if (altitude % ALTITUDE_LABEL_INTERVAL == 0) {
            char text[8];
            snprintf(text, sizeof(text), "%d", altitude);
            render_ttf_text(renderer, number_x, y - 11, text, font16, COLOR_WHITE);
        }
    }

    {
        int top = center_y - 16;
        int bottom = center_y + 16;
        int left = box_x + 22;
        int right = box_x + 91;
        Sint16 px[] = {
            (Sint16)(left - 10), (Sint16)left, (Sint16)right,
            (Sint16)right, (Sint16)left, (Sint16)(left - 10)
        };
        Sint16 py[] = {
            (Sint16)center_y, (Sint16)top, (Sint16)top,
            (Sint16)bottom, (Sint16)bottom, (Sint16)center_y
        };
        char text[16];

        filledPolygonRGBA(renderer, px, py, 6, 0, 0, 0, 255);
        polygonRGBA(renderer, px, py, 6, 255, 255, 255, 255);

        snprintf(text, sizeof(text), "%05d", cur);
        render_ttf_text_centered(renderer, (left + right) / 2, center_y - 11, text, font16, COLOR_WHITE);
    }

    if (tgt >= range_min && tgt <= range_max) {
        int target_y = center_y - (int)((tgt - cur) * px_per_foot + 0.5f);
        Sint16 px[] = {(Sint16)(box_x + 1), (Sint16)(box_x + 12), (Sint16)(box_x + 12), (Sint16)(box_x + 1)};
        Sint16 py[] = {(Sint16)target_y, (Sint16)(target_y - 8), (Sint16)(target_y + 8), (Sint16)target_y};
        polygonRGBA(renderer, px, py, 4, 231, 48, 223, 255);
    }
}

void drawAttitudeIndicator(SDL_Renderer *renderer, float pitch, float roll) {
    const int adi_left = 160;
    const int adi_top = 142;
    const int adi_width = 386;
    const int adi_height = 424;
    const int adi_right = adi_left + adi_width;
    const int adi_bottom = adi_top + adi_height;
    const int adi_center_x = adi_left + adi_width / 2;
    const int adi_center_y = adi_top + adi_height / 2;
    const int radius = 18;
    const float pitch_px = 8.0f;

    {
        SDL_Rect clip_rect = {adi_left, adi_top, adi_width + 1, adi_height + 1};
        float horizon_y = adi_center_y + pitch * pitch_px;
        int step = 0;

        // 先在矩形裁剪区内完成天地背景和俯仰刻度，再用圆角遮罩修成仪表窗形状。
        SDL_RenderSetClipRect(renderer, &clip_rect);
        boxRGBA(renderer, adi_left, adi_top, adi_right, adi_bottom, 114, 57, 5, 255);
        fill_sky_region(renderer, adi_left, adi_top, adi_right, adi_bottom, horizon_y, roll, 32, 116, 172, 255);

        {
            float dx = (float)(adi_width + 80);
            float rx1 = 0.0f;
            float ry1 = 0.0f;
            float rx2 = 0.0f;
            float ry2 = 0.0f;

            rotate_point(-dx, pitch * pitch_px, roll, &rx1, &ry1);
            rotate_point(dx, pitch * pitch_px, roll, &rx2, &ry2);
            lineRGBA(renderer,
                (int)(adi_center_x + rx1 + 0.5f), (int)(adi_center_y + ry1 + 0.5f),
                (int)(adi_center_x + rx2 + 0.5f), (int)(adi_center_y + ry2 + 0.5f),
                255, 255, 255, 255);
        }

        for (step = -8; step <= 8; ++step) {
            float mark = step * 2.5f;
            float offset = (pitch - mark) * pitch_px;
            int line_width = 28;

            if (fabsf(mark) < 0.01f) continue;
            if (fmodf(fabsf(mark), 10.0f) < 0.01f) {
                line_width = 116;
            } else if (fmodf(fabsf(mark), 5.0f) < 0.01f) {
                line_width = 50;
            }

            {
                float rx1 = 0.0f;
                float ry1 = 0.0f;
                float rx2 = 0.0f;
                float ry2 = 0.0f;

                rotate_point(-(float)line_width * 0.5f, offset, roll, &rx1, &ry1);
                rotate_point((float)line_width * 0.5f, offset, roll, &rx2, &ry2);
                lineRGBA(renderer,
                    (int)(adi_center_x + rx1 + 0.5f), (int)(adi_center_y + ry1 + 0.5f),
                    (int)(adi_center_x + rx2 + 0.5f), (int)(adi_center_y + ry2 + 0.5f),
                    255, 255, 255, 255);

                if (line_width >= 116 && fabsf(mark) < 19.9f) {
                    char label[8];
                    float left_label_x = 0.0f;
                    float left_label_y = 0.0f;
                    float right_label_x = 0.0f;
                    float right_label_y = 0.0f;

                    snprintf(label, sizeof(label), "%d", (int)(fabsf(mark) + 0.5f));
                    rotate_point(-(float)line_width * 0.5f - 22.0f, offset, roll, &left_label_x, &left_label_y);
                    rotate_point((float)line_width * 0.5f + 22.0f, offset, roll, &right_label_x, &right_label_y);
                    render_ttf_text_centered(renderer,
                        (int)(adi_center_x + left_label_x + 0.5f),
                        (int)(adi_center_y + left_label_y - 10.0f + 0.5f),
                        label, font16, COLOR_WHITE);
                    render_ttf_text_centered(renderer,
                        (int)(adi_center_x + right_label_x + 0.5f),
                        (int)(adi_center_y + right_label_y - 10.0f + 0.5f),
                        label, font16, COLOR_WHITE);
                }
            }
        }

        SDL_RenderSetClipRect(renderer, NULL);
        mask_rounded_corners(renderer, adi_left, adi_top, adi_right, adi_bottom, radius, horizon_y, roll);
        roundedRectangleRGBA(renderer, adi_left, adi_top, adi_right, adi_bottom, radius, 130, 137, 144, 255);
    }

    {
        const int bank_radius = 160;
        int degree = 0;

        for (degree = -60; degree <= 60; degree += 10) {
            int length = (degree % 30 == 0) ? 11 : 7;
            float radians = degree * PI_F / 180.0f;
            float s = sinf(radians);
            float c = cosf(radians);
            int x1 = adi_center_x + (int)(bank_radius * s + 0.5f);
            int y1 = adi_top + 36 + (int)((1.0f - c) * bank_radius + 0.5f);
            int x2 = adi_center_x + (int)((bank_radius - length) * s + 0.5f);
            int y2 = adi_top + 36 + (int)((1.0f - c) * (bank_radius - length) + 0.5f);
            lineRGBA(renderer, x1, y1, x2, y2, 255, 255, 255, 255);
        }

        filledTrigonRGBA(renderer, adi_center_x - 11, adi_top + 2, adi_center_x + 11, adi_top + 2, adi_center_x, adi_top + 14, 255, 255, 255, 255);

        {
            float marker_rad = roll * PI_F / 180.0f;
            float s = sinf(marker_rad);
            float c = cosf(marker_rad);
            float base_radius = bank_radius - 2.0f;
            float tip_radius = bank_radius - 16.0f;
            float half_width = 10.0f;
            float tx = adi_center_x + tip_radius * s;
            float ty = adi_top + 36 + (1.0f - c) * tip_radius;
            float bx = adi_center_x + base_radius * s;
            float by = adi_top + 36 + (1.0f - c) * base_radius;
            float px = s ? c : 1.0f;
            float py = -s;
            Sint16 vx[] = {
                (Sint16)(bx - half_width * px + 0.5f),
                (Sint16)(bx + half_width * px + 0.5f),
                (Sint16)(tx + half_width * 0.55f * px + 0.5f),
                (Sint16)(tx - half_width * 0.55f * px + 0.5f)
            };
            Sint16 vy[] = {
                (Sint16)(by - half_width * py + 0.5f),
                (Sint16)(by + half_width * py + 0.5f),
                (Sint16)(ty + half_width * 0.55f * py + 0.5f),
                (Sint16)(ty - half_width * 0.55f * py + 0.5f)
            };
            polygonRGBA(renderer, vx, vy, 4, 255, 255, 255, 255);
        }
    }

    {
        int horizon_y = adi_center_y + (int)(pitch * pitch_px + 0.5f);

        boxRGBA(renderer, adi_center_x - 158, horizon_y - 1, adi_center_x - 63, horizon_y + 13, 0, 0, 0, 255);
        rectangleRGBA(renderer, adi_center_x - 158, horizon_y - 1, adi_center_x - 63, horizon_y + 13, 255, 255, 255, 255);
        boxRGBA(renderer, adi_center_x + 64, horizon_y - 1, adi_center_x + 158, horizon_y + 13, 0, 0, 0, 255);
        rectangleRGBA(renderer, adi_center_x + 64, horizon_y - 1, adi_center_x + 158, horizon_y + 13, 255, 255, 255, 255);
        boxRGBA(renderer, adi_center_x - 93, horizon_y - 1, adi_center_x - 78, horizon_y + 36, 0, 0, 0, 255);
        rectangleRGBA(renderer, adi_center_x - 93, horizon_y - 1, adi_center_x - 78, horizon_y + 36, 255, 255, 255, 255);
        boxRGBA(renderer, adi_center_x + 79, horizon_y - 1, adi_center_x + 94, horizon_y + 36, 0, 0, 0, 255);
        rectangleRGBA(renderer, adi_center_x + 79, horizon_y - 1, adi_center_x + 94, horizon_y + 36, 255, 255, 255, 255);
        rectangleRGBA(renderer, adi_center_x - 5, horizon_y - 5, adi_center_x + 5, horizon_y + 5, 255, 255, 255, 255);
    }

    if (current_altitude <= 2500) {
        int box_w = 106;
        int box_h = 36;
        int box_x = adi_center_x - box_w / 2;
        int box_y = adi_bottom - 40;
        char label[16];

        snprintf(label, sizeof(label), "%d", current_altitude);
        boxRGBA(renderer, box_x, box_y, box_x + box_w, box_y + box_h, 0, 0, 0, 255);
        render_ttf_text_centered(renderer, adi_center_x, box_y + 7, label, font20, COLOR_WHITE);
    }
}

void drawVerticalSpeedIndicator(SDL_Renderer *renderer) {
    const int x = 686;
    const int y = 150;
    const int w = 44;
    const int h = 370;
    const int center_y = y + h / 2;
    const int tip_x = x + 8;
    const int end_x = x + w - 6;
    const int vsi = clamp_int(current_vertical_speed, VERTICAL_SPEED_MIN, VERTICAL_SPEED_MAX);
    const int sign = vsi >= 0 ? 1 : -1;
    const float offset = vsi_value_to_offset(vsi);
    static const int values[] = {0, 500, 1000, 2000, 4000, 6000};
    static const float offsets[] = {0.0f, 28.0f, 56.0f, 100.0f, 150.0f, 185.0f};
    int i = 0;

    draw_panel_frame(renderer, x, y, w, h, 10);
    render_ttf_text_centered(renderer, x + w / 2, y - 22, "V/S", font16, COLOR_WHITE);

    for (i = 1; i < 6; ++i) {
        int upper_y = center_y - (int)(offsets[i] + 0.5f);
        int lower_y = center_y + (int)(offsets[i] + 0.5f);
        int inner_x = (i < 3) ? x + 10 : x + 16;
        char label[8];

        lineRGBA(renderer, inner_x, upper_y, x + w - 8, upper_y, 240, 240, 240, 220);
        lineRGBA(renderer, inner_x, lower_y, x + w - 8, lower_y, 240, 240, 240, 220);
        snprintf(label, sizeof(label), "%d", values[i] >= 1000 ? values[i] / 1000 : values[i] / 100);
        render_ttf_text(renderer, x - 19, upper_y - 8, label, font12, COLOR_WHITE);
        render_ttf_text(renderer, x - 19, lower_y - 8, label, font12, COLOR_WHITE);
    }

    lineRGBA(renderer, x + 7, center_y, x + w - 8, center_y, 255, 255, 255, 255);

    {
        // 指针固定从右侧零位出发，只根据垂直速度把左端点推到对应刻度。
        int pointer_y = center_y - sign * (int)(offset + 0.5f);
        Sint16 px[] = {(Sint16)end_x, (Sint16)(end_x - 9), (Sint16)tip_x};
        Sint16 py[] = {(Sint16)center_y, (Sint16)pointer_y, (Sint16)pointer_y};
        char value_text[16];

        aalineRGBA(renderer, end_x, center_y, tip_x, pointer_y, 255, 255, 255, 255);
        filledPolygonRGBA(renderer, px, py, 3, 255, 255, 255, 255);

        snprintf(value_text, sizeof(value_text), "%d", clamp_int(vsi < 0 ? -vsi : vsi, 0, VERTICAL_SPEED_TEXT_MAX));
        if (vsi >= 0) {
            render_ttf_text_centered(renderer, x + w / 2, y + 8, value_text, font16, COLOR_GREEN);
        } else {
            render_ttf_text_centered(renderer, x + w / 2, y + h - 28, value_text, font16, COLOR_AMBER);
        }
    }
}

void drawHeadingIndicator(SDL_Renderer *renderer) {
    const int clip_top = 646;
    const int clip_height = 75;
    const int center_x = PFD_WIDTH / 2;
    const int radius = (int)(PFD_WIDTH / 3.2f + 0.5f);
    const int center_y = PFD_HEIGHT + radius - 70;
    const int current_hdg = normalize_heading(current_heading);
    const int target_hdg = normalize_heading(target_heading);
    const int diff = ((target_hdg - current_hdg + 540) % 360) - 180;
    SDL_Rect clip = {0, clip_top, PFD_WIDTH, clip_height};
    int rel = 0;

    // 罗盘圆心故意放在窗口外，只裁切出上半弧，得到 PFD 常见的“半罗盘带”效果。
    SDL_RenderSetClipRect(renderer, &clip);
    filledCircleRGBA(renderer, center_x, center_y, radius, 8, 10, 12, 245);
    circleRGBA(renderer, center_x, center_y, radius, 180, 188, 196, 255);
    circleRGBA(renderer, center_x, center_y, radius - 18, 80, 90, 98, 255);

    for (rel = -105; rel <= 105; rel += 5) {
        // rel 表示相对当前航向的角度偏移，所以当前航向始终保持在正上方，旋转的是罗盘刻度本身。
        int absolute_heading = normalize_heading(current_hdg + rel);
        int length = (absolute_heading % 30 == 0) ? 18 : 10;
        float radians = rel * PI_F / 180.0f;
        float s = sinf(radians);
        float c = cosf(radians);
        int x1 = center_x + (int)(radius * s + 0.5f);
        int y1 = center_y - (int)(radius * c + 0.5f);
        int x2 = center_x + (int)((radius - length) * s + 0.5f);
        int y2 = center_y - (int)((radius - length) * c + 0.5f);

        lineRGBA(renderer, x1, y1, x2, y2, 255, 255, 255, 255);

        if (absolute_heading % 30 == 0) {
            char label[8];
            int label_heading = absolute_heading / 10;
            int tx = center_x + (int)((radius - 38) * s + 0.5f);
            int ty = center_y - (int)((radius - 38) * c + 0.5f);

            snprintf(label, sizeof(label), "%02d", label_heading);
            render_ttf_text_centered(renderer, tx, ty - 10, label, font12, COLOR_WHITE);
        }
    }

    {
        // 目标航向 bug 只需要绘制当前航向与目标航向的相对夹角，不需要整块罗盘再旋转一次。
        float bug_rad = diff * PI_F / 180.0f;
        float s = sinf(bug_rad);
        float c = cosf(bug_rad);
        float bug_radius = radius - 8.0f;
        float inner_radius = radius - 26.0f;
        float half_width = 8.0f;
        float bx = center_x + bug_radius * s;
        float by = center_y - bug_radius * c;
        float ix = center_x + inner_radius * s;
        float iy = center_y - inner_radius * c;
        float nx = c;
        float ny = s;
        Sint16 px[] = {
            (Sint16)(bx - nx * half_width + 0.5f),
            (Sint16)(bx + nx * half_width + 0.5f),
            (Sint16)(ix + nx * half_width * 0.8f + 0.5f),
            (Sint16)(ix - nx * half_width * 0.8f + 0.5f)
        };
        Sint16 py[] = {
            (Sint16)(by + ny * half_width + 0.5f),
            (Sint16)(by - ny * half_width + 0.5f),
            (Sint16)(iy - ny * half_width * 0.8f + 0.5f),
            (Sint16)(iy + ny * half_width * 0.8f + 0.5f)
        };

        polygonRGBA(renderer, px, py, 4, 231, 48, 223, 255);
    }

    SDL_RenderSetClipRect(renderer, NULL);

    {
        Sint16 px[] = {(Sint16)(center_x - 13), (Sint16)(center_x + 13), (Sint16)center_x};
        Sint16 py[] = {(Sint16)clip_top, (Sint16)clip_top, (Sint16)(clip_top + 17)};
        filledPolygonRGBA(renderer, px, py, 3, 0, 0, 0, 255);
        polygonRGBA(renderer, px, py, 3, 255, 255, 255, 255);
    }

    {
        char current_text[16];
        char target_text[16];

        snprintf(current_text, sizeof(current_text), "%03d", current_hdg);
        snprintf(target_text, sizeof(target_text), "SEL %03d", target_hdg);
        render_ttf_text_centered(renderer, center_x, clip_top + 25, current_text, font20, COLOR_WHITE);
        render_ttf_text_centered(renderer, center_x, clip_top + 47, target_text, font12, COLOR_MAGENTA);
        render_ttf_text_centered(renderer, center_x + 94, clip_top + 47, "MAG", font12, COLOR_GREEN);
    }
}

void drawThrustIndicator(SDL_Renderer *renderer) {
    const int center_x = 132;
    const int center_y = 633;
    const int radius = 74;
    const float start_deg = 202.5f;
    const float sweep_deg = 225.0f;
    const int thrust = clamp_int(current_thrust, THRUST_MIN, THRUST_MAX);
    int value = 0;

    draw_panel_frame(renderer, 34, 556, 196, 135, 12);
    render_ttf_text(renderer, 48, 565, "THRUST", font12, COLOR_WHITE);
    render_ttf_text(renderer, 166, 565, "%", font12, COLOR_WHITE);

    arcRGBA(renderer, center_x, center_y, radius, (Sint16)start_deg, (Sint16)(start_deg + sweep_deg), 180, 188, 196, 255);
    arcRGBA(renderer, center_x, center_y, radius - 1, (Sint16)start_deg, (Sint16)(start_deg + sweep_deg), 180, 188, 196, 255);

    for (value = 0; value <= 100; value += 20) {
        // 0~100% 被映射到 225 度弧段，每 20% 一个主刻度。
        float degrees = start_deg + sweep_deg * ((float)value / 100.0f);
        float radians = degrees * PI_F / 180.0f;
        float s = cosf(radians);
        float c = sinf(radians);
        int tick_len = value == 100 ? 16 : 12;
        int x1 = center_x + (int)(radius * s + 0.5f);
        int y1 = center_y + (int)(radius * c + 0.5f);
        int x2 = center_x + (int)((radius - tick_len) * s + 0.5f);
        int y2 = center_y + (int)((radius - tick_len) * c + 0.5f);
        char label[8];

        lineRGBA(renderer, x1, y1, x2, y2, 255, 255, 255, 255);

        snprintf(label, sizeof(label), "%d", value);
        render_ttf_text_centered(renderer,
            center_x + (int)((radius - 28) * s + 0.5f),
            center_y + (int)((radius - 28) * c + 0.5f) - 8,
            label, font12, COLOR_WHITE);
    }

    {
        // 当前推力沿同一套角度换算画指针，保证指针与刻度共用一套几何规则。
        float degrees = start_deg + sweep_deg * ((float)thrust / 100.0f);
        float radians = degrees * PI_F / 180.0f;
        float s = cosf(radians);
        float c = sinf(radians);
        int x2 = center_x + (int)((radius - 22) * s + 0.5f);
        int y2 = center_y + (int)((radius - 22) * c + 0.5f);
        char label[16];

        aalineRGBA(renderer, center_x, center_y, x2, y2, 255, 188, 64, 255);
        filledCircleRGBA(renderer, center_x, center_y, 5, 255, 188, 64, 255);

        snprintf(label, sizeof(label), "%d", thrust);
        render_ttf_text(renderer, 52, 596, label, font24, COLOR_AMBER);
    }
}

void drawPFDStatus(SDL_Renderer *renderer) {
    char source_text[16];
    const char *status_text = current_xplane_connected ? "UDP OK" : "SIM";

    switch (current_data_source) {
        case PFD_SOURCE_FILE:
            strcpy(source_text, "FILE");
            break;
        case PFD_SOURCE_XPLANE:
            strcpy(source_text, "XPLANE");
            break;
        default:
            strcpy(source_text, "STATIC");
            break;
    }

    draw_label_value(renderer, 604, 18, "SOURCE", source_text, COLOR_WHITE, COLOR_GREEN);
    draw_label_value(renderer, 604, 58, "STATUS", status_text, COLOR_WHITE, current_xplane_connected ? COLOR_GREEN : COLOR_AMBER);
}
