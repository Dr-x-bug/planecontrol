#pragma once
#include <SDL2/SDL.h>
#include "config.h"

enum FMCKey {
    FMC_KEY_L1=0,FMC_KEY_L2,FMC_KEY_L3,FMC_KEY_L4,FMC_KEY_L5,FMC_KEY_L6,
    FMC_KEY_R1,FMC_KEY_R2,FMC_KEY_R3,FMC_KEY_R4,FMC_KEY_R5,FMC_KEY_R6,
    FMC_KEY_INIT_REF,FMC_KEY_RTE,FMC_KEY_CLB,FMC_KEY_CRZ,FMC_KEY_DES,
    FMC_KEY_DIR_INTC,FMC_KEY_LEGS,FMC_KEY_DEP_ARR,FMC_KEY_HOLD,FMC_KEY_PROG,
    FMC_KEY_FIX,FMC_KEY_NAV_RAD,FMC_KEY_PREV_PAGE,FMC_KEY_NEXT_PAGE,
    FMC_KEY_EXEC,
    FMC_KEY_A,FMC_KEY_B,FMC_KEY_C,FMC_KEY_D,FMC_KEY_E,
    FMC_KEY_F,FMC_KEY_G,FMC_KEY_H,FMC_KEY_I,FMC_KEY_J,
    FMC_KEY_K,FMC_KEY_L,FMC_KEY_M,FMC_KEY_N,FMC_KEY_O,
    FMC_KEY_P,FMC_KEY_Q,FMC_KEY_R,FMC_KEY_S,FMC_KEY_T,
    FMC_KEY_U,FMC_KEY_V,FMC_KEY_W,FMC_KEY_X,FMC_KEY_Y,FMC_KEY_Z,
    FMC_KEY_1,FMC_KEY_2,FMC_KEY_3,FMC_KEY_4,FMC_KEY_5,FMC_KEY_6,
    FMC_KEY_7,FMC_KEY_8,FMC_KEY_9,FMC_KEY_DOT,FMC_KEY_0,FMC_KEY_PLUS_MINUS,
    FMC_KEY_SP,FMC_KEY_DEL,FMC_KEY_SLASH,FMC_KEY_CLR,
    FMC_KEY_COUNT
};

struct FMCButton;
typedef void (*FMCButtonCallback)(FMCButton* btn);

struct FMCButton {
    FMCKey    key;
    FMCShape  shape;
    SDL_Rect  rect;
    int       radius;
    const char* label;
    int       state;
    FMCButtonCallback on_click;
    SDL_Color color_normal;
    SDL_Color color_hover;

    bool hit_test(int fx, int fy) const {
        if (shape == FMC_SHAPE_CIRCLE) {
            int cx=rect.x+rect.w/2, cy=rect.y+rect.h/2;
            int dx=fx-cx, dy=fy-cy;
            return (dx*dx+dy*dy) <= radius*radius;
        }
        return fx>=rect.x && fx<=rect.x+rect.w && fy>=rect.y && fy<=rect.y+rect.h;
    }
};

extern FMCButton fmc_buttons[FMC_KEY_COUNT];
void fmc_buttons_init();

extern char fmc_lines_L[6][24];
extern char fmc_lines_R[6][24];
extern char fmc_title[32];
extern char fmc_scratchpad[32];
extern bool fmc_exec_light;

void fmc_on_lsk(FMCButton* btn);
void fmc_on_func_key(FMCButton* btn);
void fmc_on_letter(FMCButton* btn);
void fmc_on_number(FMCButton* btn);
void fmc_on_edit(FMCButton* btn);
void fmc_on_exec(FMCButton* btn);

inline int fmc_hit_test(int mx, int my, float scale, int ww, int wh) {
    int dw=(int)(FMC_BASE_W*scale), dh=(int)(FMC_BASE_H*scale);
    int ox=(ww-dw)/2, oy=(wh-dh)/2;
    int fx=(int)((mx-ox)/scale), fy=(int)((my-oy)/scale);
    for (int i=0;i<FMC_KEY_COUNT;i++) {
        if (fmc_buttons[i].shape==FMC_SHAPE_NONE) continue;
        if (fmc_buttons[i].hit_test(fx,fy)) return i;
    }
    return -1;
}

inline void fmc_update_hover(int idx) {
    for (int i=0;i<FMC_KEY_COUNT;i++) fmc_buttons[i].state&=~FMC_STATE_HOVER;
    if (idx>=0) fmc_buttons[idx].state|=FMC_STATE_HOVER;
}

inline void fmc_handle_click(int idx) {
    if (idx<0||idx>=FMC_KEY_COUNT) return;
    FMCButton& b=fmc_buttons[idx];
    b.state|=FMC_STATE_PRESSED;
    if (b.on_click) b.on_click(&b);
}
