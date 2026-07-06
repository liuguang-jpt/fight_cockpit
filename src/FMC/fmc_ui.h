#ifndef FMC_UI_H
#define FMC_UI_H

#include "fmc_data.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define FMC_WIDTH 638
#define FMC_HEIGHT 998
#define FMC_BUTTON_COUNT 70

typedef enum FMCButtonId {
    FMC_BTN_NONE = 0,
    FMC_BTN_L1, FMC_BTN_L2, FMC_BTN_L3, FMC_BTN_L4, FMC_BTN_L5, FMC_BTN_L6,
    FMC_BTN_R1, FMC_BTN_R2, FMC_BTN_R3, FMC_BTN_R4, FMC_BTN_R5, FMC_BTN_R6,
    FMC_BTN_INIT_REF, FMC_BTN_RTE, FMC_BTN_CLB, FMC_BTN_CRZ, FMC_BTN_DES,
    FMC_BTN_DIR_INTC, FMC_BTN_LEGS, FMC_BTN_DEP_ARR, FMC_BTN_HOLD, FMC_BTN_PROG,
    FMC_BTN_FIX, FMC_BTN_NAV_RAD, FMC_BTN_PREV_PAGE, FMC_BTN_NEXT_PAGE,
    FMC_BTN_EXEC, FMC_BTN_DEL, FMC_BTN_CLR, FMC_BTN_SLASH,
    FMC_BTN_A, FMC_BTN_B, FMC_BTN_C, FMC_BTN_D, FMC_BTN_E, FMC_BTN_F, FMC_BTN_G, FMC_BTN_H, FMC_BTN_I, FMC_BTN_J,
    FMC_BTN_K, FMC_BTN_L, FMC_BTN_M, FMC_BTN_N, FMC_BTN_O, FMC_BTN_P, FMC_BTN_Q, FMC_BTN_R, FMC_BTN_S, FMC_BTN_T,
    FMC_BTN_U, FMC_BTN_V, FMC_BTN_W, FMC_BTN_X, FMC_BTN_Y, FMC_BTN_Z,
    FMC_BTN_1, FMC_BTN_2, FMC_BTN_3, FMC_BTN_4, FMC_BTN_5, FMC_BTN_6, FMC_BTN_7, FMC_BTN_8, FMC_BTN_9,
    FMC_BTN_DOT, FMC_BTN_0, FMC_BTN_PLUS_MINUS
} FMCButtonId;

typedef void (*FMCButtonHandler)(FMCButtonId id);

typedef struct FMCButton {
    FMCButtonId id;
    SDL_Rect rect;
    const char *label;
    int hovered;
    FMCButtonHandler handler;
} FMCButton;

extern TTF_Font *fmc_font18;
extern TTF_Font *fmc_font22;
extern TTF_Font *fmc_font26;
extern SDL_Texture *fmc_background_texture;
extern SDL_Texture *fmc_logic_texture;
extern SDL_Rect fmc_render_rect;
extern int fmc_window_width;
extern int fmc_window_height;
extern FMCButton fmc_buttons[FMC_BUTTON_COUNT];
extern int fmc_button_total;

int initFMC(void);
SDL_Window *createFMCWindow(void);
SDL_Renderer *createFMCRenderer(SDL_Window *window);
SDL_Texture *createFMCLogicTexture(SDL_Renderer *renderer);
void destroyFMC(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *logic_texture);
void init_fmc_buttons(void);
void draw_fmc(SDL_Renderer *renderer);
void fmc_update_hover(int x, int y);
void fmc_click_button_at(int x, int y);
void fmc_handle_text_input(const char *text);
void fmc_handle_backspace(void);
void fmc_handle_enter(void);

#endif
