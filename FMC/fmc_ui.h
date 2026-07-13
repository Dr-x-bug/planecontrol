/**
 * fmc_ui.h — FMC 用户界面组件 (按键定义、交互回调)
 *
 * ========== FMC 按键布局 ==========
 * 真实 Boeing 737 FMC 面板包含约55个按键, 分为5组:
 *   1. 行选键 (LSK)  — L1-L6 (左6个) + R1-R6 (右6个)  = 12个
 *   2. 功能键         — INIT REF/RTE/CLB/CRZ/DES/DEP ARR等 = 14个
 *   3. 字母键         — A-Z = 26个
 *   4. 数字键         — 0-9 + . + +/- = 12个
 *   5. 编辑键         — SP/DEL/CLR/SLASH = 4个
 *   6. 执行键         — EXEC = 1个
 *
 * ========== FMCButton 结构 ==========
 * 每个按键包含:
 *   - key:   枚举标识 (用于switch分发)
 *   - shape: 形状 (矩形/圆形)
 *   - rect:  边界矩形 (用于碰撞检测)
 *   - label: 显示标签 (如 "A", "INIT REF")
 *   - state: 位标志 (NORMAL/HOVER/PRESSED/ACTIVE)
 *   - on_click: 回调函数指针
 *   - color_normal/color_hover: 颜色状态
 *
 * ========== 交互流程 ==========
 *   fmc_hit_test()    → 鼠标坐标 → 按键索引 (碰撞检测)
 *   fmc_update_hover() → 更新悬停高亮状态
 *   fmc_handle_click() → 按键按下 → 调用 on_click 回调
 *
 *   回调分发:
 *     fmc_on_lsk()      → 行选键统一处理 (内部按页面分发)
 *     fmc_on_func_key() → 功能键 (页面切换/PREV NEXT翻页)
 *     fmc_on_letter()   → 字母键 → g_screen.type_char()
 *     fmc_on_number()   → 数字键 → g_screen.type_char()
 *     fmc_on_edit()     → 编辑键 → g_screen.backspace()/clear_scratchpad()
 *     fmc_on_exec()     → EXEC键 → 参数校验 + 数据同步
 */

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

// 分组回调 (内部使用)
void fmc_on_lsk(FMCButton* btn);
void fmc_on_func_key(FMCButton* btn);
void fmc_on_letter(FMCButton* btn);
void fmc_on_number(FMCButton* btn);
void fmc_on_edit(FMCButton* btn);
void fmc_on_exec(FMCButton* btn);

// === 独立回调函数 (每个按键一个) ===
// 屏幕左右选择键 L1-L6
void fmc_handle_screen_l1(FMCButton* btn);
void fmc_handle_screen_l2(FMCButton* btn);
void fmc_handle_screen_l3(FMCButton* btn);
void fmc_handle_screen_l4(FMCButton* btn);
void fmc_handle_screen_l5(FMCButton* btn);
void fmc_handle_screen_l6(FMCButton* btn);
// 屏幕左右选择键 R1-R6
void fmc_handle_screen_r1(FMCButton* btn);
void fmc_handle_screen_r2(FMCButton* btn);
void fmc_handle_screen_r3(FMCButton* btn);
void fmc_handle_screen_r4(FMCButton* btn);
void fmc_handle_screen_r5(FMCButton* btn);
void fmc_handle_screen_r6(FMCButton* btn);
// 功能按键
void fmc_handle_init_ref(FMCButton* btn);
void fmc_handle_rte(FMCButton* btn);
void fmc_handle_clb(FMCButton* btn);
void fmc_handle_crz(FMCButton* btn);
void fmc_handle_des(FMCButton* btn);
void fmc_handle_dir_intc(FMCButton* btn);
void fmc_handle_legs(FMCButton* btn);
void fmc_handle_dep_arr(FMCButton* btn);
void fmc_handle_hold(FMCButton* btn);
void fmc_handle_prog(FMCButton* btn);
void fmc_handle_fix(FMCButton* btn);
void fmc_handle_nav_rad(FMCButton* btn);
void fmc_handle_prev_page(FMCButton* btn);
void fmc_handle_next_page(FMCButton* btn);
// 字母按键 A-Z
void fmc_handle_a(FMCButton* btn);
void fmc_handle_b(FMCButton* btn);
void fmc_handle_c(FMCButton* btn);
void fmc_handle_d(FMCButton* btn);
void fmc_handle_e(FMCButton* btn);
void fmc_handle_f(FMCButton* btn);
void fmc_handle_g(FMCButton* btn);
void fmc_handle_h(FMCButton* btn);
void fmc_handle_i(FMCButton* btn);
void fmc_handle_j(FMCButton* btn);
void fmc_handle_k(FMCButton* btn);
void fmc_handle_l(FMCButton* btn);
void fmc_handle_m(FMCButton* btn);
void fmc_handle_n(FMCButton* btn);
void fmc_handle_o(FMCButton* btn);
void fmc_handle_p(FMCButton* btn);
void fmc_handle_q(FMCButton* btn);
void fmc_handle_r(FMCButton* btn);
void fmc_handle_s(FMCButton* btn);
void fmc_handle_t(FMCButton* btn);
void fmc_handle_u(FMCButton* btn);
void fmc_handle_v(FMCButton* btn);
void fmc_handle_w(FMCButton* btn);
void fmc_handle_x(FMCButton* btn);
void fmc_handle_y(FMCButton* btn);
void fmc_handle_z(FMCButton* btn);
// 数字/符号按键
void fmc_handle_1(FMCButton* btn);
void fmc_handle_2(FMCButton* btn);
void fmc_handle_3(FMCButton* btn);
void fmc_handle_4(FMCButton* btn);
void fmc_handle_5(FMCButton* btn);
void fmc_handle_6(FMCButton* btn);
void fmc_handle_7(FMCButton* btn);
void fmc_handle_8(FMCButton* btn);
void fmc_handle_9(FMCButton* btn);
void fmc_handle_0(FMCButton* btn);
void fmc_handle_dot(FMCButton* btn);
void fmc_handle_plus_minus(FMCButton* btn);
// 编辑按键
void fmc_handle_sp(FMCButton* btn);
void fmc_handle_del(FMCButton* btn);
void fmc_handle_slash(FMCButton* btn);
void fmc_handle_clr(FMCButton* btn);
// 执行按键
void fmc_handle_exec(FMCButton* btn);

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
