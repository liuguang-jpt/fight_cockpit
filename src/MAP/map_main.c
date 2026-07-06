#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_ttf.h>

#define COCKPIT_WIDTH 8026
#define COCKPIT_HEIGHT 3136
#define COCKPIT_BG_PATH "assets/assets/assets/main.png"
#define COCKPIT_FONT_PATH "assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF"
#define COCKPIT_FULLSCREEN_ICON_PATH "assets/assets/assets/full_screen.png"
#define COCKPIT_WINDOW_ICON_PATH "assets/assets/assets/window.png"
#define MODULE_COUNT 5

typedef struct CockpitModule {
    const char *name;
    const char *window_title;
    const char *exe_path;
    SDL_Rect logical_rect;
    SDL_Color color;
    PROCESS_INFORMATION process;
    HWND window;
    int attached;
    int launch_failed;
} CockpitModule;

typedef struct FindWindowContext {
    DWORD process_id;
    const char *title;
    HWND result;
} FindWindowContext;

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_background = NULL;
static SDL_Texture *g_fullscreen_icon = NULL;
static SDL_Texture *g_window_icon = NULL;
static TTF_Font *g_font16 = NULL;
static TTF_Font *g_font22 = NULL;
static HWND g_host_hwnd = NULL;
static int g_window_width = 1600;
static int g_window_height = 900;
static int g_is_fullscreen = 1;
static int g_is_dragging = 0;
static int g_drag_last_x = 0;
static int g_drag_last_y = 0;
static float g_scale = 1.0f;
static float g_min_scale = 1.0f;
static float g_max_scale = 1.0f;
static float g_offset_x = 0.0f;
static float g_offset_y = 0.0f;

static CockpitModule g_modules[MODULE_COUNT] = {
    {"PFD", "PFD", "build\\pfd.exe", {1210, 905, 900, 910}, {77, 219, 255, 255}, {0}, NULL, 0, 0},
    {"ND", "ND", "build\\nd.exe", {2120, 905, 900, 910}, {231, 48, 223, 255}, {0}, NULL, 0, 0},
    {"EICAS1", "EICAS1", "build\\eicas1.exe", {3075, 905, 1060, 910}, {80, 236, 124, 255}, {0}, NULL, 0, 0},
    {"EICAS2", "EICAS2", "build\\eicas2.exe", {4740, 905, 900, 910}, {255, 188, 64, 255}, {0}, NULL, 0, 0},
    {"FMC", "FMC", "build\\fmc.exe", {3470, 1710, 820, 1280}, {255, 255, 255, 255}, {0}, NULL, 0, 0}
};

static SDL_Rect get_fullscreen_button_rect(void) {
    SDL_Rect rect = {g_window_width - 112, 24, 40, 40};
    return rect;
}

static SDL_Rect get_window_button_rect(void) {
    SDL_Rect rect = {g_window_width - 60, 24, 40, 40};
    return rect;
}

static int point_in_rect(int x, int y, const SDL_Rect *rect) {
    if (rect == NULL) return 0;
    return x >= rect->x && x < rect->x + rect->w && y >= rect->y && y < rect->y + rect->h;
}

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void render_text(SDL_Renderer *renderer, TTF_Font *font, int x, int y, SDL_Color color, const char *text) {
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

static SDL_Texture *load_texture(SDL_Renderer *renderer, const char *path) {
    SDL_Surface *surface = IMG_Load(path);
    SDL_Texture *texture = NULL;

    if (surface == NULL) return NULL;
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

/* The whole cockpit stays on one logical canvas, then zoom/pan projects it into the desktop window. */
static void clamp_view(void) {
    float scaled_w = COCKPIT_WIDTH * g_scale;
    float scaled_h = COCKPIT_HEIGHT * g_scale;

    if (scaled_w <= (float)g_window_width) g_offset_x = ((float)g_window_width - scaled_w) * 0.5f;
    else g_offset_x = clamp_float(g_offset_x, (float)g_window_width - scaled_w, 0.0f);

    if (scaled_h <= (float)g_window_height) g_offset_y = ((float)g_window_height - scaled_h) * 0.5f;
    else g_offset_y = clamp_float(g_offset_y, (float)g_window_height - scaled_h, 0.0f);
}

static void reset_view(void) {
    float scale_x = (float)g_window_width / (float)COCKPIT_WIDTH;
    float scale_y = (float)g_window_height / (float)COCKPIT_HEIGHT;

    g_min_scale = scale_x < scale_y ? scale_x : scale_y;
    g_max_scale = g_min_scale * 2.5f;
    g_scale = g_min_scale;
    g_offset_x = ((float)g_window_width - (float)COCKPIT_WIDTH * g_scale) * 0.5f;
    g_offset_y = ((float)g_window_height - (float)COCKPIT_HEIGHT * g_scale) * 0.5f;
    clamp_view();
}

static void update_window_size(void) {
    SDL_GetWindowSize(g_window, &g_window_width, &g_window_height);
    if (g_window_width < 1) g_window_width = 1;
    if (g_window_height < 1) g_window_height = 1;
    reset_view();
}

static SDL_Rect logical_to_window_rect(SDL_Rect logical_rect) {
    SDL_Rect rect;

    rect.x = (int)lroundf(g_offset_x + (float)logical_rect.x * g_scale);
    rect.y = (int)lroundf(g_offset_y + (float)logical_rect.y * g_scale);
    rect.w = (int)lroundf((float)logical_rect.w * g_scale);
    rect.h = (int)lroundf((float)logical_rect.h * g_scale);
    return rect;
}

static void zoom_at_point(int mouse_x, int mouse_y, float factor) {
    float new_scale = clamp_float(g_scale * factor, g_min_scale, g_max_scale);
    float logical_x;
    float logical_y;

    if (fabsf(new_scale - g_scale) < 0.0001f) return;
    logical_x = ((float)mouse_x - g_offset_x) / g_scale;
    logical_y = ((float)mouse_y - g_offset_y) / g_scale;
    g_offset_x = (float)mouse_x - logical_x * new_scale;
    g_offset_y = (float)mouse_y - logical_y * new_scale;
    g_scale = new_scale;
    clamp_view();
}

static void toggle_fullscreen(int enable) {
    Uint32 flags = enable ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;

    SDL_SetWindowFullscreen(g_window, flags);
    g_is_fullscreen = enable;
    SDL_Delay(50);
    update_window_size();
}

static int init_cockpit(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) return -1;
    if (TTF_Init() == -1) return -1;
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG) return -1;

    g_font16 = TTF_OpenFont(COCKPIT_FONT_PATH, 16);
    g_font22 = TTF_OpenFont(COCKPIT_FONT_PATH, 22);
    if (g_font16 == NULL || g_font22 == NULL) return -1;

    g_window = SDL_CreateWindow(
        "Cockpit Host",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        g_window_width,
        g_window_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP
    );
    if (g_window == NULL) return -1;

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == NULL) return -1;

    g_background = load_texture(g_renderer, COCKPIT_BG_PATH);
    g_fullscreen_icon = load_texture(g_renderer, COCKPIT_FULLSCREEN_ICON_PATH);
    g_window_icon = load_texture(g_renderer, COCKPIT_WINDOW_ICON_PATH);
    if (g_background == NULL || g_fullscreen_icon == NULL || g_window_icon == NULL) return -1;

    update_window_size();
    return 0;
}

static void destroy_cockpit(void) {
    int i;

    for (i = 0; i < MODULE_COUNT; ++i) {
        if (g_modules[i].window != NULL && IsWindow(g_modules[i].window)) SendMessage(g_modules[i].window, WM_CLOSE, 0, 0);
        if (g_modules[i].process.hProcess != NULL) {
            if (WaitForSingleObject(g_modules[i].process.hProcess, 1500) == WAIT_TIMEOUT) TerminateProcess(g_modules[i].process.hProcess, 0);
            CloseHandle(g_modules[i].process.hProcess);
            g_modules[i].process.hProcess = NULL;
        }
        if (g_modules[i].process.hThread != NULL) {
            CloseHandle(g_modules[i].process.hThread);
            g_modules[i].process.hThread = NULL;
        }
    }

    if (g_background != NULL) SDL_DestroyTexture(g_background);
    if (g_fullscreen_icon != NULL) SDL_DestroyTexture(g_fullscreen_icon);
    if (g_window_icon != NULL) SDL_DestroyTexture(g_window_icon);
    if (g_renderer != NULL) SDL_DestroyRenderer(g_renderer);
    if (g_window != NULL) SDL_DestroyWindow(g_window);
    if (g_font16 != NULL) TTF_CloseFont(g_font16);
    if (g_font22 != NULL) TTF_CloseFont(g_font22);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

static HWND get_sdl_hwnd(SDL_Window *window) {
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return NULL;
    return info.info.win.window;
}

static BOOL CALLBACK find_process_window_proc(HWND hwnd, LPARAM lparam) {
    FindWindowContext *ctx = (FindWindowContext *)lparam;
    DWORD process_id = 0;
    char title[128];

    GetWindowThreadProcessId(hwnd, &process_id);
    if (process_id != ctx->process_id) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    GetWindowTextA(hwnd, title, sizeof(title));
    if (_stricmp(title, ctx->title) != 0) return TRUE;
    ctx->result = hwnd;
    return FALSE;
}

static HWND find_module_window(DWORD process_id, const char *title) {
    FindWindowContext ctx;

    ctx.process_id = process_id;
    ctx.title = title;
    ctx.result = NULL;
    EnumWindows(find_process_window_proc, (LPARAM)&ctx);
    return ctx.result;
}

static void reposition_module(CockpitModule *module) {
    SDL_Rect rect;
    LONG_PTR style;

    if (module == NULL || module->window == NULL || !IsWindow(module->window)) return;
    rect = logical_to_window_rect(module->logical_rect);
    if (rect.w < 32 || rect.h < 32) {
        ShowWindow(module->window, SW_HIDE);
        return;
    }

    style = GetWindowLongPtr(module->window, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_POPUP);
    style |= WS_CHILD | WS_VISIBLE;
    SetWindowLongPtr(module->window, GWL_STYLE, style);
    SetParent(module->window, g_host_hwnd);
    MoveWindow(module->window, rect.x, rect.y, rect.w, rect.h, TRUE);
    ShowWindow(module->window, SW_SHOW);
}

static void launch_module(CockpitModule *module) {
    STARTUPINFOA startup_info;
    char command[MAX_PATH];

    if (module == NULL) return;
    if (GetFileAttributesA(module->exe_path) == INVALID_FILE_ATTRIBUTES) {
        module->launch_failed = 1;
        return;
    }

    memset(&startup_info, 0, sizeof(startup_info));
    memset(&module->process, 0, sizeof(module->process));
    startup_info.cb = sizeof(startup_info);
    snprintf(command, sizeof(command), "%s", module->exe_path);

    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &module->process)) {
        module->launch_failed = 1;
        return;
    }
}

/* Child module windows are discovered and attached lazily so the host can manage their full lifecycle. */
static void attach_modules_if_ready(void) {
    int i;

    for (i = 0; i < MODULE_COUNT; ++i) {
        CockpitModule *module = &g_modules[i];

        if (module->launch_failed || module->attached || module->process.dwProcessId == 0) continue;
        module->window = find_module_window(module->process.dwProcessId, module->window_title);
        if (module->window == NULL) continue;
        reposition_module(module);
        module->attached = 1;
    }
}

static void reposition_all_modules(void) {
    int i;

    for (i = 0; i < MODULE_COUNT; ++i) {
        if (g_modules[i].attached) reposition_module(&g_modules[i]);
    }
}

static void draw_module_overlay(const CockpitModule *module) {
    SDL_Rect rect;
    SDL_Color text_color = {245, 245, 245, 255};
    char status[32];

    if (module == NULL) return;
    rect = logical_to_window_rect(module->logical_rect);
    if (rect.w < 32 || rect.h < 32) return;

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, module->color.r, module->color.g, module->color.b, 34);
    SDL_RenderFillRect(g_renderer, &rect);
    SDL_SetRenderDrawColor(g_renderer, module->color.r, module->color.g, module->color.b, 220);
    SDL_RenderDrawRect(g_renderer, &rect);

    if (module->launch_failed) snprintf(status, sizeof(status), "MISSING");
    else if (module->attached) snprintf(status, sizeof(status), "ATTACHED");
    else snprintf(status, sizeof(status), "BOOTING");

    render_text(g_renderer, g_font22, rect.x + 14, rect.y + 12, text_color, module->name);
    render_text(g_renderer, g_font16, rect.x + 14, rect.y + 42, module->color, status);
}

static void draw_controls(void) {
    SDL_Rect full_rect = get_fullscreen_button_rect();
    SDL_Rect window_rect = get_window_button_rect();
    SDL_Color text_color = {240, 240, 240, 255};
    char view_text[64];

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 8, 12, 16, 200);
    SDL_Rect bar = {18, 18, 320, 58};
    SDL_RenderFillRect(g_renderer, &bar);
    SDL_SetRenderDrawColor(g_renderer, 120, 130, 140, 255);
    SDL_RenderDrawRect(g_renderer, &bar);

    snprintf(view_text, sizeof(view_text), "Mouse wheel zoom  Drag pan  F11 toggle  %.0f%%", g_scale / g_min_scale * 100.0f);
    render_text(g_renderer, g_font16, 30, 38, text_color, view_text);
    SDL_RenderCopy(g_renderer, g_fullscreen_icon, NULL, &full_rect);
    SDL_RenderCopy(g_renderer, g_window_icon, NULL, &window_rect);
}

static void draw_frame(void) {
    int i;
    SDL_Rect background_rect = {
        (int)lroundf(g_offset_x),
        (int)lroundf(g_offset_y),
        (int)lroundf((float)COCKPIT_WIDTH * g_scale),
        (int)lroundf((float)COCKPIT_HEIGHT * g_scale)
    };

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_background, NULL, &background_rect);

    for (i = 0; i < MODULE_COUNT; ++i) draw_module_overlay(&g_modules[i]);
    draw_controls();
}

static void capture_frame_if_requested(void) {
    const char *capture_path = getenv("COCKPIT_CAPTURE_PATH");
    SDL_Surface *surface = NULL;

    if (capture_path == NULL || *capture_path == '\0') return;
    surface = SDL_CreateRGBSurfaceWithFormat(0, g_window_width, g_window_height, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL) return;
    if (SDL_RenderReadPixels(g_renderer, NULL, SDL_PIXELFORMAT_RGBA32, surface->pixels, surface->pitch) == -1) {
        SDL_FreeSurface(surface);
        return;
    }
    SDL_SaveBMP(surface, capture_path);
    SDL_FreeSurface(surface);
}

int main(int argc, char *argv[]) {
    SDL_Event event;
    Uint32 start_ticks = 0;
    Uint32 capture_delay = 1200;
    int quit = 0;
    int i;

    (void)argc;
    (void)argv;

    if (init_cockpit() == -1) {
        destroy_cockpit();
        return -1;
    }

    g_host_hwnd = get_sdl_hwnd(g_window);
    if (g_host_hwnd == NULL) {
        destroy_cockpit();
        return -1;
    }

    for (i = 0; i < MODULE_COUNT; ++i) launch_module(&g_modules[i]);
    start_ticks = SDL_GetTicks();

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) toggle_fullscreen(!g_is_fullscreen);
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_0) reset_view();

            if (event.type == SDL_MOUSEWHEEL) {
                int mouse_x = 0;
                int mouse_y = 0;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                zoom_at_point(mouse_x, mouse_y, event.wheel.y > 0 ? 1.1f : 0.9f);
                reposition_all_modules();
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                SDL_Rect full_rect = get_fullscreen_button_rect();
                SDL_Rect window_rect = get_window_button_rect();

                if (point_in_rect(event.button.x, event.button.y, &full_rect)) toggle_fullscreen(1);
                else if (point_in_rect(event.button.x, event.button.y, &window_rect)) toggle_fullscreen(0);
                else {
                    g_is_dragging = 1;
                    g_drag_last_x = event.button.x;
                    g_drag_last_y = event.button.y;
                }
            }

            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) g_is_dragging = 0;

            if (event.type == SDL_MOUSEMOTION && g_is_dragging) {
                g_offset_x += (float)(event.motion.x - g_drag_last_x);
                g_offset_y += (float)(event.motion.y - g_drag_last_y);
                g_drag_last_x = event.motion.x;
                g_drag_last_y = event.motion.y;
                clamp_view();
                reposition_all_modules();
            }

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                g_window_width = event.window.data1;
                g_window_height = event.window.data2;
                reset_view();
                reposition_all_modules();
            }
        }

        attach_modules_if_ready();
        reposition_all_modules();
        draw_frame();
        if (getenv("COCKPIT_CAPTURE_PATH") != NULL && SDL_GetTicks() - start_ticks > capture_delay) capture_frame_if_requested();
        SDL_RenderPresent(g_renderer);

        if (getenv("COCKPIT_CAPTURE_PATH") != NULL && SDL_GetTicks() - start_ticks > capture_delay) {
            quit = 1;
        }
        SDL_Delay(16);
    }

    destroy_cockpit();
    return 0;
}
