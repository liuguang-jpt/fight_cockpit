#include "fmc_ui.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

TTF_Font *fmc_font18 = NULL;
TTF_Font *fmc_font22 = NULL;
TTF_Font *fmc_font26 = NULL;
SDL_Texture *fmc_background_texture = NULL;
SDL_Texture *fmc_logic_texture = NULL;
SDL_Rect fmc_render_rect = {0, 0, FMC_WIDTH, FMC_HEIGHT};
int fmc_window_width = FMC_WIDTH;
int fmc_window_height = FMC_HEIGHT;
FMCButton fmc_buttons[FMC_BUTTON_COUNT];
int fmc_button_total = 0;

static const SDL_Color FMC_WHITE = {240, 240, 240, 255};
static const SDL_Color FMC_CYAN = {0, 231, 255, 255};
static const SDL_Color FMC_GREEN = {80, 236, 124, 255};
static const SDL_Color FMC_AMBER = {210, 165, 120, 255};
static const SDL_Color FMC_HOVER = {255, 255, 255, 40};

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

    if (font == NULL || text == NULL || *text == '\0') return;
    if (TTF_SizeUTF8(font, text, &text_w, NULL) == -1) return;
    render_text(renderer, center_x - text_w / 2, y, text, font, color);
}

static void set_status_page(FMCButtonId id) { (void)id; fmc_set_page(FMC_PAGE_STATUS); }
static void set_menu_page(FMCButtonId id) { (void)id; fmc_set_page(FMC_PAGE_MENU); }
static void set_rte_page(FMCButtonId id) { (void)id; fmc_set_page(FMC_PAGE_RTE); }
static void set_clb_page(FMCButtonId id) { (void)id; fmc_set_page(FMC_PAGE_CLB); }
static void set_crz_page(FMCButtonId id) { (void)id; fmc_set_page(FMC_PAGE_CRZ); }
static void set_des_page(FMCButtonId id) { (void)id; fmc_set_page(FMC_PAGE_DES); }
static void set_dep_arr_page(FMCButtonId id) { (void)id; fmc_set_page(FMC_PAGE_DEP_ARR); }

static void add_button(FMCButtonId id, int x, int y, int w, int h, const char *label, FMCButtonHandler handler) {
    if (fmc_button_total >= FMC_BUTTON_COUNT) return;
    fmc_buttons[fmc_button_total].id = id;
    fmc_buttons[fmc_button_total].rect.x = x;
    fmc_buttons[fmc_button_total].rect.y = y;
    fmc_buttons[fmc_button_total].rect.w = w;
    fmc_buttons[fmc_button_total].rect.h = h;
    fmc_buttons[fmc_button_total].label = label;
    fmc_buttons[fmc_button_total].hovered = 0;
    fmc_buttons[fmc_button_total].handler = handler;
    ++fmc_button_total;
}

static void page_button_handler(FMCButtonId id) {
    switch (id) {
        case FMC_BTN_INIT_REF: set_menu_page(id); break;
        case FMC_BTN_PROG: set_status_page(id); break;
        case FMC_BTN_RTE: set_rte_page(id); break;
        case FMC_BTN_CLB: set_clb_page(id); break;
        case FMC_BTN_CRZ: set_crz_page(id); break;
        case FMC_BTN_DES: set_des_page(id); break;
        case FMC_BTN_DEP_ARR: set_dep_arr_page(id); break;
        case FMC_BTN_PREV_PAGE:
            if (fmc_current_page == FMC_PAGE_RTE && fmc_route_page > 0) --fmc_route_page;
            else if (fmc_current_page == FMC_PAGE_DEP_ARR && fmc_dep_arr_page > 0) --fmc_dep_arr_page;
            break;
        case FMC_BTN_NEXT_PAGE:
            if (fmc_current_page == FMC_PAGE_RTE && (fmc_route_page + 1) * 5 < (int)fmc_route_count) ++fmc_route_page;
            else if (fmc_current_page == FMC_PAGE_DEP_ARR && fmc_dep_arr_page < 1) ++fmc_dep_arr_page;
            break;
        default: break;
    }
}

static void scratchpad_button_handler(FMCButtonId id) {
    char text[4] = "";

    switch (id) {
        case FMC_BTN_CLR: fmc_clear_scratchpad(); return;
        case FMC_BTN_DEL: fmc_backspace_scratchpad(); return;
        case FMC_BTN_SLASH: snprintf(text, sizeof(text), "/"); break;
        case FMC_BTN_DOT: snprintf(text, sizeof(text), "."); break;
        case FMC_BTN_PLUS_MINUS: snprintf(text, sizeof(text), "-"); break;
        default:
            if (id >= FMC_BTN_A && id <= FMC_BTN_Z) snprintf(text, sizeof(text), "%c", 'A' + (id - FMC_BTN_A));
            else if (id >= FMC_BTN_1 && id <= FMC_BTN_9) snprintf(text, sizeof(text), "%c", '1' + (id - FMC_BTN_1));
            else if (id == FMC_BTN_0) snprintf(text, sizeof(text), "0");
            break;
    }
    if (text[0] != '\0') fmc_append_scratchpad_text(text);
}

static void lsk_handler(FMCButtonId id) {
    size_t route_index = 0;

    /* LSK behavior is page-dependent, matching how the real unit reuses keys. */
    if (id == FMC_BTN_EXEC) {
        fmc_commit_exec();
        return;
    }

    if (fmc_current_page == FMC_PAGE_RTE) {
        if (id == FMC_BTN_L1) {
            if (fmc_scratchpad[0] != '\0' && fmc_stage_airport(1, fmc_scratchpad)) fmc_clear_scratchpad();
            return;
        }
        if (id == FMC_BTN_R1) {
            if (fmc_scratchpad[0] != '\0' && fmc_stage_airport(0, fmc_scratchpad)) fmc_clear_scratchpad();
            return;
        }
        if (id >= FMC_BTN_L2 && id <= FMC_BTN_L6 && fmc_scratchpad[0] != '\0') {
            route_index = (size_t)fmc_route_page * 5 + (size_t)(id - FMC_BTN_L2);
            if (route_index < FMC_ROUTE_CAPACITY && fmc_append_route_waypoint(fmc_scratchpad)) fmc_clear_scratchpad();
            return;
        }
    }

    if (fmc_current_page == FMC_PAGE_CLB && id == FMC_BTN_L1 && fmc_scratchpad[0] != '\0') {
        fmc_climb_data.speed = atoi(fmc_scratchpad);
        fmc_clear_scratchpad();
        return;
    }
    if (fmc_current_page == FMC_PAGE_CLB && id == FMC_BTN_R1 && fmc_scratchpad[0] != '\0') {
        fmc_climb_data.altitude = atoi(fmc_scratchpad);
        fmc_clear_scratchpad();
        return;
    }
    if (fmc_current_page == FMC_PAGE_CRZ && id == FMC_BTN_L1 && fmc_scratchpad[0] != '\0') {
        fmc_cruise_data.speed = atoi(fmc_scratchpad);
        fmc_clear_scratchpad();
        return;
    }
    if (fmc_current_page == FMC_PAGE_CRZ && id == FMC_BTN_R1 && fmc_scratchpad[0] != '\0') {
        fmc_cruise_data.altitude = atoi(fmc_scratchpad);
        fmc_clear_scratchpad();
        return;
    }
    if (fmc_current_page == FMC_PAGE_DES && id == FMC_BTN_L1 && fmc_scratchpad[0] != '\0') {
        fmc_descent_data.speed = atoi(fmc_scratchpad);
        fmc_clear_scratchpad();
        return;
    }
    if (fmc_current_page == FMC_PAGE_DES && id == FMC_BTN_R1 && fmc_scratchpad[0] != '\0') {
        fmc_descent_data.vref = atoi(fmc_scratchpad);
        fmc_clear_scratchpad();
        return;
    }

    if (fmc_current_page == FMC_PAGE_DEP_ARR) {
        switch (id) {
            case FMC_BTN_L1: fmc_cycle_dep_arr_option(0, 1); break;
            case FMC_BTN_L2: fmc_cycle_dep_arr_option(1, 1); break;
            case FMC_BTN_L3: fmc_cycle_dep_arr_option(2, 1); break;
            case FMC_BTN_R1: fmc_cycle_dep_arr_option(3, 1); break;
            case FMC_BTN_R2: fmc_cycle_dep_arr_option(4, 1); break;
            case FMC_BTN_R3: fmc_cycle_dep_arr_option(5, 1); break;
            default: break;
        }
    }
}

static void draw_hover_overlay(SDL_Renderer *renderer) {
    int i;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (i = 0; i < fmc_button_total; ++i) {
        if (!fmc_buttons[i].hovered) continue;
        SDL_SetRenderDrawColor(renderer, FMC_HOVER.r, FMC_HOVER.g, FMC_HOVER.b, FMC_HOVER.a);
        SDL_RenderFillRect(renderer, &fmc_buttons[i].rect);
    }
}

static void draw_page_common(SDL_Renderer *renderer, const char *title, const char *sub_title) {
    render_text(renderer, 126, 88, title, fmc_font22, FMC_CYAN);
    render_text(renderer, 420, 88, sub_title, fmc_font18, FMC_GREEN);
}

static void draw_menu_page(SDL_Renderer *renderer) {
    char info[32];

    draw_page_common(renderer, "FMC MENU", "INIT/REF");
    render_text(renderer, 128, 138, "<STATUS", fmc_font18, FMC_WHITE);
    render_text(renderer, 128, 178, "<ROUTE", fmc_font18, FMC_WHITE);
    render_text(renderer, 128, 218, "<DEP/ARR", fmc_font18, FMC_WHITE);
    render_text(renderer, 128, 258, "<CLB/CRZ/DES", fmc_font18, FMC_WHITE);
    snprintf(info, sizeof(info), "APT %d  FIX %d", fmc_airport_count, fmc_fix_count);
    render_text(renderer, 128, 336, info, fmc_font18, FMC_GREEN);
}

static void draw_status_page(SDL_Renderer *renderer) {
    char line[48];

    draw_page_common(renderer, "FMC STATUS", "PROG");
    render_text(renderer, 128, 138, "ORIGIN", fmc_font18, FMC_WHITE);
    render_text(renderer, 396, 138, fmc_origin, fmc_font18, FMC_CYAN);
    render_text(renderer, 128, 178, "DEST", fmc_font18, FMC_WHITE);
    render_text(renderer, 396, 178, fmc_destination, fmc_font18, FMC_CYAN);
    render_text(renderer, 128, 218, "ROUTE COUNT", fmc_font18, FMC_WHITE);
    snprintf(line, sizeof(line), "%u", (unsigned int)fmc_route_count);
    render_text(renderer, 396, 218, line, fmc_font18, FMC_GREEN);
    render_text(renderer, 128, 258, "STATUS", fmc_font18, FMC_WHITE);
    render_text(renderer, 320, 258, fmc_status_message, fmc_font18, FMC_GREEN);
}

static void draw_rte_page(SDL_Renderer *renderer) {
    char line[48];
    int i;

    draw_page_common(renderer, "RTE 1", "RTE");
    render_text(renderer, 112, 126, "ORIGIN", fmc_font18, FMC_WHITE);
    render_text(renderer, 128, 148, fmc_exec_armed && fmc_pending_origin[0] != '\0' ? fmc_pending_origin : fmc_origin, fmc_font22, FMC_CYAN);
    render_text(renderer, 428, 126, "DEST", fmc_font18, FMC_WHITE);
    render_text(renderer, 414, 148, fmc_exec_armed && fmc_pending_destination[0] != '\0' ? fmc_pending_destination : fmc_destination, fmc_font22, FMC_CYAN);

    for (i = 0; i < 5; ++i) {
        size_t index = (size_t)fmc_route_page * 5 + (size_t)i;
        int y = 192 + i * 40;

        snprintf(line, sizeof(line), "%d", i + 1 + fmc_route_page * 5);
        render_text(renderer, 128, y, line, fmc_font18, FMC_WHITE);
        if (index < fmc_route_count) {
            snprintf(line, sizeof(line), "%s %s", fmc_route[index].name, fmc_route[index].kind);
            render_text(renderer, 168, y, line, fmc_font18, FMC_GREEN);
        } else {
            render_text(renderer, 168, y, "-----", fmc_font18, FMC_WHITE);
        }
    }

    snprintf(line, sizeof(line), "%d/%d", fmc_route_page + 1, (int)((fmc_route_count + 4) / 5 == 0 ? 1 : (fmc_route_count + 4) / 5));
    render_text(renderer, 446, 342, line, fmc_font18, FMC_WHITE);
}

static void draw_phase_page(SDL_Renderer *renderer, const char *title, const FMCPhaseData *data, const char *right_label) {
    char line[32];

    draw_page_common(renderer, title, title);
    render_text(renderer, 128, 138, "SPD", fmc_font18, FMC_WHITE);
    snprintf(line, sizeof(line), "%d", data->speed);
    render_text(renderer, 128, 160, line, fmc_font22, FMC_CYAN);
    render_text(renderer, 428, 138, right_label, fmc_font18, FMC_WHITE);
    snprintf(line, sizeof(line), "%d", right_label[0] == 'V' ? data->vref : data->altitude);
    render_text(renderer, 428, 160, line, fmc_font22, FMC_CYAN);
    if (data->transition_altitude > 0) {
        render_text(renderer, 128, 220, "TRANS ALT", fmc_font18, FMC_WHITE);
        snprintf(line, sizeof(line), "%d", data->transition_altitude);
        render_text(renderer, 128, 242, line, fmc_font22, FMC_GREEN);
    }
}

static void draw_dep_arr_page(SDL_Renderer *renderer) {
    if (fmc_dep_arr_page == 1) {
        draw_page_common(renderer, "DEP/ARR ACT", "DEP ARR");
        render_text(renderer, 128, 138, "DEP RWY", fmc_font18, FMC_WHITE);
        render_text(renderer, 404, 138, "ARR RWY", fmc_font18, FMC_WHITE);
        render_text(renderer, 128, 168, fmc_proc_options.dep_runways[fmc_proc_options.dep_runway_index], fmc_font18, FMC_GREEN);
        render_text(renderer, 404, 168, fmc_proc_options.arr_runways[fmc_proc_options.arr_runway_index], fmc_font18, FMC_GREEN);
        render_text(renderer, 128, 228, "SID", fmc_font18, FMC_WHITE);
        render_text(renderer, 404, 228, "STAR", fmc_font18, FMC_WHITE);
        render_text(renderer, 128, 258, fmc_proc_options.dep_sids[fmc_proc_options.dep_sid_index], fmc_font18, FMC_GREEN);
        render_text(renderer, 404, 258, fmc_proc_options.arr_stars[fmc_proc_options.arr_star_index], fmc_font18, FMC_GREEN);
        render_text(renderer, 128, 318, "TRANS", fmc_font18, FMC_WHITE);
        render_text(renderer, 404, 318, "IAF", fmc_font18, FMC_WHITE);
        render_text(renderer, 128, 348, fmc_proc_options.dep_transitions[fmc_proc_options.dep_transition_index], fmc_font18, FMC_GREEN);
        render_text(renderer, 404, 348, fmc_proc_options.arr_transitions[fmc_proc_options.arr_transition_index], fmc_font18, FMC_GREEN);
        return;
    }

    draw_page_common(renderer, "DEP/ARR INDEX", "DEP ARR");
    render_text(renderer, 112, 126, "DEP", fmc_font18, FMC_WHITE);
    render_text(renderer, 176, 126, fmc_origin, fmc_font18, FMC_CYAN);
    render_text(renderer, 404, 126, "ARR", fmc_font18, FMC_WHITE);
    render_text(renderer, 458, 126, fmc_destination, fmc_font18, FMC_CYAN);

    render_text(renderer, 128, 168, fmc_proc_options.dep_runways[fmc_proc_options.dep_runway_index], fmc_font18, FMC_GREEN);
    render_text(renderer, 128, 208, fmc_proc_options.dep_sids[fmc_proc_options.dep_sid_index], fmc_font18, FMC_GREEN);
    render_text(renderer, 128, 248, fmc_proc_options.dep_transitions[fmc_proc_options.dep_transition_index], fmc_font18, FMC_GREEN);
    render_text(renderer, 404, 168, fmc_proc_options.arr_runways[fmc_proc_options.arr_runway_index], fmc_font18, FMC_GREEN);
    render_text(renderer, 404, 208, fmc_proc_options.arr_stars[fmc_proc_options.arr_star_index], fmc_font18, FMC_GREEN);
    render_text(renderer, 404, 248, fmc_proc_options.arr_transitions[fmc_proc_options.arr_transition_index], fmc_font18, FMC_GREEN);
}

static void draw_display(SDL_Renderer *renderer) {
    switch (fmc_current_page) {
        case FMC_PAGE_MENU: draw_menu_page(renderer); break;
        case FMC_PAGE_STATUS: draw_status_page(renderer); break;
        case FMC_PAGE_RTE: draw_rte_page(renderer); break;
        case FMC_PAGE_CLB: draw_phase_page(renderer, "CLB", &fmc_climb_data, "ALT"); break;
        case FMC_PAGE_CRZ: draw_phase_page(renderer, "CRZ", &fmc_cruise_data, "ALT"); break;
        case FMC_PAGE_DES: draw_phase_page(renderer, "DES", &fmc_descent_data, "VREF"); break;
        case FMC_PAGE_DEP_ARR: draw_dep_arr_page(renderer); break;
        default: break;
    }

    render_text(renderer, 128, 392, fmc_status_message, fmc_font18, FMC_GREEN);
    render_text(renderer, 128, 416, fmc_scratchpad[0] == '\0' ? "[]" : fmc_scratchpad, fmc_font22, FMC_CYAN);
    if (fmc_exec_armed) render_text(renderer, 464, 88, "MOD", fmc_font18, FMC_AMBER);
}

int initFMC(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) return -1;
    if (TTF_Init() == -1) return -1;
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG) return -1;

    fmc_font18 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 18);
    fmc_font22 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 22);
    fmc_font26 = TTF_OpenFont("assets/assets/assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 26);
    if (fmc_font18 == NULL || fmc_font22 == NULL || fmc_font26 == NULL) return -1;
    return 0;
}

SDL_Window *createFMCWindow(void) {
    return SDL_CreateWindow("FMC", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, fmc_window_width, fmc_window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
}

SDL_Renderer *createFMCRenderer(SDL_Window *window) {
    return SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
}

SDL_Texture *createFMCLogicTexture(SDL_Renderer *renderer) {
    SDL_Surface *surface = IMG_Load("assets/assets/assets/fmc.png");
    if (surface == NULL) return NULL;
    fmc_background_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (fmc_background_texture == NULL) return NULL;
    return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, FMC_WIDTH, FMC_HEIGHT);
}

void destroyFMC(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *logic_texture) {
    if (fmc_font18 != NULL) TTF_CloseFont(fmc_font18);
    if (fmc_font22 != NULL) TTF_CloseFont(fmc_font22);
    if (fmc_font26 != NULL) TTF_CloseFont(fmc_font26);
    if (fmc_background_texture != NULL) SDL_DestroyTexture(fmc_background_texture);
    if (logic_texture != NULL) SDL_DestroyTexture(logic_texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void init_fmc_buttons(void) {
    int i;
    int x;
    int y;

    fmc_button_total = 0;
    add_button(FMC_BTN_L1, 6, 118, 50, 34, "L1", lsk_handler);
    add_button(FMC_BTN_L2, 6, 166, 50, 34, "L2", lsk_handler);
    add_button(FMC_BTN_L3, 6, 214, 50, 34, "L3", lsk_handler);
    add_button(FMC_BTN_L4, 6, 262, 50, 34, "L4", lsk_handler);
    add_button(FMC_BTN_L5, 6, 310, 50, 34, "L5", lsk_handler);
    add_button(FMC_BTN_L6, 6, 358, 50, 34, "L6", lsk_handler);
    add_button(FMC_BTN_R1, 582, 118, 50, 34, "R1", lsk_handler);
    add_button(FMC_BTN_R2, 582, 166, 50, 34, "R2", lsk_handler);
    add_button(FMC_BTN_R3, 582, 214, 50, 34, "R3", lsk_handler);
    add_button(FMC_BTN_R4, 582, 262, 50, 34, "R4", lsk_handler);
    add_button(FMC_BTN_R5, 582, 310, 50, 34, "R5", lsk_handler);
    add_button(FMC_BTN_R6, 582, 358, 50, 34, "R6", lsk_handler);
    add_button(FMC_BTN_EXEC, 541, 557, 78, 50, "EXEC", lsk_handler);

    add_button(FMC_BTN_INIT_REF, 69, 480, 74, 50, "INIT", page_button_handler);
    add_button(FMC_BTN_RTE, 151, 480, 74, 50, "RTE", page_button_handler);
    add_button(FMC_BTN_CLB, 233, 480, 74, 50, "CLB", page_button_handler);
    add_button(FMC_BTN_CRZ, 315, 480, 74, 50, "CRZ", page_button_handler);
    add_button(FMC_BTN_DES, 397, 480, 74, 50, "DES", page_button_handler);
    add_button(FMC_BTN_DIR_INTC, 69, 559, 74, 50, "DIR", page_button_handler);
    add_button(FMC_BTN_LEGS, 151, 559, 74, 50, "LEGS", page_button_handler);
    add_button(FMC_BTN_DEP_ARR, 233, 559, 74, 50, "DEPARR", page_button_handler);
    add_button(FMC_BTN_HOLD, 315, 559, 74, 50, "HOLD", page_button_handler);
    add_button(FMC_BTN_PROG, 397, 559, 74, 50, "PROG", page_button_handler);
    add_button(FMC_BTN_FIX, 69, 638, 74, 50, "FIX", page_button_handler);
    add_button(FMC_BTN_NAV_RAD, 151, 638, 74, 50, "NAV", page_button_handler);
    add_button(FMC_BTN_PREV_PAGE, 69, 717, 74, 50, "PREV", page_button_handler);
    add_button(FMC_BTN_NEXT_PAGE, 151, 717, 74, 50, "NEXT", page_button_handler);

    for (i = 0; i < 26; ++i) {
        x = 262 + (i % 5) * 69;
        y = 614 + (i / 5) * 63;
        add_button((FMCButtonId)(FMC_BTN_A + i), x, y, 48, 48, NULL, scratchpad_button_handler);
    }

    add_button(FMC_BTN_1, 52, 740, 52, 52, "1", scratchpad_button_handler);
    add_button(FMC_BTN_2, 115, 740, 52, 52, "2", scratchpad_button_handler);
    add_button(FMC_BTN_3, 178, 740, 52, 52, "3", scratchpad_button_handler);
    add_button(FMC_BTN_4, 52, 802, 52, 52, "4", scratchpad_button_handler);
    add_button(FMC_BTN_5, 115, 802, 52, 52, "5", scratchpad_button_handler);
    add_button(FMC_BTN_6, 178, 802, 52, 52, "6", scratchpad_button_handler);
    add_button(FMC_BTN_7, 52, 866, 52, 52, "7", scratchpad_button_handler);
    add_button(FMC_BTN_8, 115, 866, 52, 52, "8", scratchpad_button_handler);
    add_button(FMC_BTN_9, 178, 866, 52, 52, "9", scratchpad_button_handler);
    add_button(FMC_BTN_DOT, 52, 930, 52, 44, ".", scratchpad_button_handler);
    add_button(FMC_BTN_0, 115, 930, 52, 44, "0", scratchpad_button_handler);
    add_button(FMC_BTN_PLUS_MINUS, 178, 930, 52, 44, "+/-", scratchpad_button_handler);
    add_button(FMC_BTN_DEL, 437, 930, 48, 44, "DEL", scratchpad_button_handler);
    add_button(FMC_BTN_SLASH, 491, 930, 48, 44, "/", scratchpad_button_handler);
    add_button(FMC_BTN_CLR, 545, 930, 48, 44, "CLR", scratchpad_button_handler);
}

void draw_fmc(SDL_Renderer *renderer) {
    SDL_RenderCopy(renderer, fmc_background_texture, NULL, NULL);
    draw_display(renderer);
    draw_hover_overlay(renderer);
}

void fmc_update_hover(int x, int y) {
    int i;

    for (i = 0; i < fmc_button_total; ++i) {
        SDL_Rect rect = fmc_buttons[i].rect;
        fmc_buttons[i].hovered = x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
    }
}

void fmc_click_button_at(int x, int y) {
    int i;

    for (i = 0; i < fmc_button_total; ++i) {
        SDL_Rect rect = fmc_buttons[i].rect;
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
            if (fmc_buttons[i].handler != NULL) fmc_buttons[i].handler(fmc_buttons[i].id);
            return;
        }
    }
}

void fmc_handle_text_input(const char *text) {
    fmc_append_scratchpad_text(text);
}

void fmc_handle_backspace(void) {
    fmc_backspace_scratchpad();
}

void fmc_handle_enter(void) {
    fmc_commit_exec();
}
