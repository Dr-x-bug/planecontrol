/**
 * fmc_ui.h — FMC 模块界面层头文件
 * 职责: 声明窗口/渲染器结构、颜色常量、FMC按键/按钮、屏幕状态、页面定义
 */
#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <string>

// ============================================================
//  一、窗口与显示参数
// ============================================================

constexpr int FMC_BASE_W      = 638;
constexpr int FMC_BASE_H      = 998;
constexpr int WIN_W           = 830;
constexpr int WIN_H           = 1298;
constexpr int SCREEN_WIDTH    = 500;
constexpr int SCREEN_HEIGHT   = 24;
constexpr int FUNCTION_WIDTH  = 68;
constexpr int FUNCTION_HEIGHT = 38;
constexpr int LETTER_LENGTH   = 52;
constexpr int NUMB_R          = 26;

// ============================================================
//  二、颜色常量
// ============================================================

namespace Color {
    constexpr SDL_Color FMC_TEXT    = {255, 255, 255, 255};
    constexpr SDL_Color FMC_WHITE   = {255, 255, 255, 255};
    constexpr SDL_Color FMC_GREEN   = {  0, 255,   0, 255};
    constexpr SDL_Color FMC_CYAN    = {  0, 220, 220, 255};
    constexpr SDL_Color FMC_AMBER   = {255, 180,   0, 255};
    constexpr SDL_Color FMC_MAGENTA = {255,   0, 255, 255};
}

// ============================================================
//  三、FMC 形状与状态枚举
// ============================================================

enum FMCShape { FMC_SHAPE_NONE, FMC_SHAPE_RECT, FMC_SHAPE_CIRCLE };

enum {
    FMC_STATE_NORMAL  = 0,
    FMC_STATE_HOVER   = 1,
    FMC_STATE_PRESSED = 2,
    FMC_STATE_ACTIVE  = 4,
};

// ============================================================
//  四、FMC 按键枚举
// ============================================================

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

// ============================================================
//  五、FMC 按钮结构
// ============================================================

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

// ============================================================
//  六、FMC 渲染器
// ============================================================

struct FMCRenderer {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* sdl_rend = nullptr;
    TTF_Font*     font     = nullptr;
    TTF_Font*     font_sm  = nullptr;
    SDL_Texture*  fmc_tex  = nullptr;
    float         scale    = 1.3f;

    bool init();
    void update_scale(int ww, int wh);
    void clear();
    void present();

    int  to_sx(int x);
    int  to_sy(int y);
    int  to_sw(int w);
    int  to_sh(int h);

    void draw_bg();
    void draw_text(int x, int y, const std::string& txt, SDL_Color c, bool sm=false);
    void draw_text_right(int rx, int y, const std::string& txt, SDL_Color c, bool sm=false);
    void draw_text_center(int cx, int y, const std::string& txt, SDL_Color c, bool sm=false);
    void draw_btn_rect(int x, int y, int w, int h, int state);
    void draw_btn_circle(int cx, int cy, int r, int state);

    ~FMCRenderer();
};

// ============================================================
//  七、FMC 页面系统
// ============================================================

enum FMCPage {
    PAGE_INIT_REF = 0,
    PAGE_RTE,
    PAGE_CLB,
    PAGE_CRZ,
    PAGE_DES,
    PAGE_DEP_ARR,
    PAGE_LEGS,
    PAGE_HOLD,
    PAGE_PROG,
    PAGE_FIX,
    PAGE_NAV_RAD,
    PAGE_COUNT
};

struct FMCScreen;

typedef void (*PageDrawFunc)(struct FMCScreen* scr);

struct PageDef {
    FMCPage       id;
    const char*   title;
    PageDrawFunc  draw;
};

// ============================================================
//  八、FMC 屏幕状态
// ============================================================

struct FMCScreen {
    FMCPage   current_page;
    bool      need_redraw;

    char line_L[6][24];
    char line_R[6][24];
    char scratchpad[32];
    bool exec_light;

    char origin[8];
    char dest[8];
    char flt_no[16];
    char crz_alt[8];
    char cost_idx[8];

    char clb_spd[8];
    char clb_spd_rest[16];
    char clb_alt_rest[8];
    char clb_rate[8];
    char clb_trans_alt[8];

    char crz_turb_n1[8];
    char crz_opt_alt[8];
    char crz_max_alt[8];
    char crz_step[8];
    char crz_wind[16];

    char des_spd[8];
    char des_spd_rest[16];
    char des_alt_rest[8];
    char des_rate[8];
    char des_trans_lvl[8];

    FMCScreen();
    void clear_all();
    bool type_char(char ch);
    void backspace();
};

extern FMCScreen g_screen;
extern PageDef   g_pages[PAGE_COUNT];

// ============================================================
//  九、FMC 界面函数声明
// ============================================================

// 按键初始化与事件处理
void fmc_buttons_init();
void fmc_on_lsk(FMCButton* btn);
void fmc_on_func_key(FMCButton* btn);
int  fmc_hit_test(int mx, int my, float scale, int win_w, int win_h);
void fmc_update_hover(int idx);
void fmc_handle_click(int idx);

// 页面切换
void fmc_switch_page(FMCPage page);

// 屏幕绘制
void fmc_draw_screen(FMCRenderer& renderer);

// 页面绘制函数
void page_draw_init_ref(FMCScreen* scr);
void page_draw_rte(FMCScreen* scr);
void page_draw_clb(FMCScreen* scr);
void page_draw_crz(FMCScreen* scr);
void page_draw_des(FMCScreen* scr);
void page_draw_dep_arr(FMCScreen* scr);
void page_draw_default(FMCScreen* scr);
