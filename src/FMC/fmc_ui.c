/**
 * fmc_ui.c — FMC 模块界面层实现
 * 职责: FMCRenderer 方法、按钮系统、页面绘制、屏幕管理
 * 过渡期: 部分实现引用原 FMC/ 目录 .cpp 文件
 */
#include "fmc_ui.h"
#include "fmc_data.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// 过渡期: 引用原模块中的页面绘制实现
#include "../FMC/fmc_pages.h"

// ============================================================
//  FMCRenderer 方法实现
// ============================================================

bool FMCRenderer::init() {
    SDL_Init(SDL_INIT_VIDEO); TTF_Init(); IMG_Init(IMG_INIT_PNG);
    window = SDL_CreateWindow("FMC",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    sdl_rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    font    = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 16);
    font_sm = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 13);
    SDL_Surface* s = IMG_Load("assets/fmc.png");
    if (s) { fmc_tex = SDL_CreateTextureFromSurface(sdl_rend, s); SDL_FreeSurface(s); }
    return window && sdl_rend && font && fmc_tex;
}

void FMCRenderer::update_scale(int ww, int wh) {
    float sx = ww / (float)FMC_BASE_W, sy = wh / (float)FMC_BASE_H;
    scale = (sx < sy) ? sx : sy;
}

void FMCRenderer::clear() {
    SDL_SetRenderDrawColor(sdl_rend, 0, 0, 0, 255);
    SDL_RenderClear(sdl_rend);
}

void FMCRenderer::present() { SDL_RenderPresent(sdl_rend); }

int FMCRenderer::to_sx(int x) {
    int wx, wy; SDL_GetWindowSize(window, &wx, &wy);
    return (int)(x * scale) + (wx - (int)(FMC_BASE_W * scale)) / 2;
}
int FMCRenderer::to_sy(int y) {
    int wx, wy; SDL_GetWindowSize(window, &wx, &wy);
    return (int)(y * scale) + (wy - (int)(FMC_BASE_H * scale)) / 2;
}
int FMCRenderer::to_sw(int w) { return (int)(w * scale); }
int FMCRenderer::to_sh(int h) { return (int)(h * scale); }

void FMCRenderer::draw_bg() {
    int dw = to_sw(FMC_BASE_W), dh = to_sh(FMC_BASE_H);
    SDL_Rect d = {to_sx(0), to_sy(0), dw, dh};
    SDL_RenderCopy(sdl_rend, fmc_tex, nullptr, &d);
}

void FMCRenderer::draw_text(int x, int y, const std::string& txt, SDL_Color c, bool sm) {
    if (txt.empty()) return;
    TTF_Font* f = sm ? font_sm : font;
    SDL_Surface* surf = TTF_RenderText_Blended(f, txt.c_str(), c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_rend, surf);
    SDL_Rect dst = {to_sx(x), to_sy(y), to_sw(surf->w), to_sh(surf->h)};
    SDL_RenderCopy(sdl_rend, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void FMCRenderer::draw_text_right(int rx, int y, const std::string& txt, SDL_Color c, bool sm) {
    if (txt.empty()) return;
    TTF_Font* f = sm ? font_sm : font;
    SDL_Surface* surf = TTF_RenderText_Blended(f, txt.c_str(), c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_rend, surf);
    SDL_Rect dst = {to_sx(rx) - to_sw(surf->w), to_sy(y), to_sw(surf->w), to_sh(surf->h)};
    SDL_RenderCopy(sdl_rend, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void FMCRenderer::draw_text_center(int cx, int y, const std::string& txt, SDL_Color c, bool sm) {
    if (txt.empty()) return;
    TTF_Font* f = sm ? font_sm : font;
    SDL_Surface* surf = TTF_RenderText_Blended(f, txt.c_str(), c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_rend, surf);
    SDL_Rect dst = {to_sx(cx) - to_sw(surf->w)/2, to_sy(y), to_sw(surf->w), to_sh(surf->h)};
    SDL_RenderCopy(sdl_rend, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void FMCRenderer::draw_btn_rect(int x, int y, int w, int h, int state) {
    SDL_Color c = (state & FMC_STATE_HOVER) ? Color::FMC_CYAN : Color::FMC_GREEN;
    SDL_SetRenderDrawColor(sdl_rend, c.r, c.g, c.b, 80);
    SDL_Rect r = {to_sx(x), to_sy(y), to_sw(w), to_sh(h)};
    SDL_RenderFillRect(sdl_rend, &r);
}

void FMCRenderer::draw_btn_circle(int cx, int cy, int r, int state) {
    SDL_Color c = (state & FMC_STATE_HOVER) ? Color::FMC_CYAN : Color::FMC_GREEN;
    filledCircleRGBA(sdl_rend, to_sx(cx), to_sy(cy), to_sw(r),
                     c.r, c.g, c.b, 80);
}

FMCRenderer::~FMCRenderer() {
    if (fmc_tex)  SDL_DestroyTexture(fmc_tex);
    if (font_sm)  TTF_CloseFont(font_sm);
    if (font)     TTF_CloseFont(font);
    if (sdl_rend) SDL_DestroyRenderer(sdl_rend);
    if (window)   SDL_DestroyWindow(window);
}

// ============================================================
//  FMCScreen 方法实现
// ============================================================

FMCScreen::FMCScreen() : current_page(PAGE_INIT_REF), need_redraw(true), exec_light(false) {
    clear_all();
}

void FMCScreen::clear_all() {
    memset(line_L, 0, sizeof(line_L));
    memset(line_R, 0, sizeof(line_R));
    memset(scratchpad, 0, sizeof(scratchpad));
    memset(origin, 0, sizeof(origin));
    memset(dest, 0, sizeof(dest));
    memset(flt_no, 0, sizeof(flt_no));
    memset(crz_alt, 0, sizeof(crz_alt));
    memset(cost_idx, 0, sizeof(cost_idx));
    memset(clb_spd, 0, sizeof(clb_spd));
    memset(clb_spd_rest, 0, sizeof(clb_spd_rest));
    memset(clb_alt_rest, 0, sizeof(clb_alt_rest));
    memset(clb_rate, 0, sizeof(clb_rate));
    memset(clb_trans_alt, 0, sizeof(clb_trans_alt));
    memset(crz_turb_n1, 0, sizeof(crz_turb_n1));
    memset(crz_opt_alt, 0, sizeof(crz_opt_alt));
    memset(crz_max_alt, 0, sizeof(crz_max_alt));
    memset(crz_step, 0, sizeof(crz_step));
    memset(crz_wind, 0, sizeof(crz_wind));
    memset(des_spd, 0, sizeof(des_spd));
    memset(des_spd_rest, 0, sizeof(des_spd_rest));
    memset(des_alt_rest, 0, sizeof(des_alt_rest));
    memset(des_rate, 0, sizeof(des_rate));
    memset(des_trans_lvl, 0, sizeof(des_trans_lvl));
}

bool FMCScreen::type_char(char ch) {
    int len = (int)strlen(scratchpad);
    if (len >= 23) return false;
    scratchpad[len] = ch;
    scratchpad[len + 1] = '\0';
    return true;
}

void FMCScreen::backspace() {
    int len = (int)strlen(scratchpad);
    if (len > 0) scratchpad[len - 1] = '\0';
}

// ============================================================
//  全局变量定义
// ============================================================

FMCButton fmc_buttons[FMC_KEY_COUNT];
FMCScreen g_screen;

// 页面定义表 (绘制函数引用原 fmc_pages.h 实现)
PageDef g_pages[PAGE_COUNT] = {
    {PAGE_INIT_REF, "INIT/REF INDEX", page_draw_init_ref},
    {PAGE_RTE,      "RTE",            page_draw_rte},
    {PAGE_CLB,      "CLB",            page_draw_clb},
    {PAGE_CRZ,      "CRZ",            page_draw_crz},
    {PAGE_DES,      "DES",            page_draw_des},
    {PAGE_DEP_ARR,  "DEP/ARR",        page_draw_dep_arr},
    {PAGE_LEGS,     "LEGS",           page_draw_default},
    {PAGE_HOLD,     "HOLD",           page_draw_default},
    {PAGE_PROG,     "PROG",           page_draw_default},
    {PAGE_FIX,      "FIX",            page_draw_default},
    {PAGE_NAV_RAD,  "NAV RAD",        page_draw_default},
};

// ============================================================
//  按钮初始化与事件处理
//  过渡期: 完整实现位于 ../FMC/fmc_buttons_init.cpp 和 fmc_buttons.cpp
// ============================================================

// 以下函数的具体实现暂保留在原 FMC/ 目录,
// 后续将逐步迁移至此文件:
//   - fmc_buttons_init()     → 当前在 fmc_buttons_init.cpp
//   - fmc_on_lsk()           → 当前在 fmc_buttons.cpp
//   - fmc_on_func_key()      → 当前在 fmc_buttons.cpp
//   - fmc_hit_test()         → 当前在 fmc_buttons.cpp
//   - fmc_update_hover()     → 当前在 fmc_buttons.cpp
//   - fmc_handle_click()     → 当前在 fmc_buttons.cpp
//   - fmc_switch_page()      → 当前在 main.cpp (通过 fmc_pages.h)
//   - fmc_draw_screen()      → 当前在 main.cpp (通过 fmc_pages.h)
