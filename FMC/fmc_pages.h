#pragma once
#include "renderer.h"

// ===== FMC 页面枚举 =====
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

// DEP/ARR 模式切换
extern char g_deparr_mode;

// INIT/REF 子页面状态
extern int g_init_subpage;

// 前向声明
struct FMCScreen;

typedef void (*PageDrawFunc)(struct FMCScreen* scr);

// ===== 页面数据 =====
struct PageDef {
    FMCPage       id;
    const char*   title;
    PageDrawFunc  draw;
};

// ===== FMC 屏幕状态 =====
struct FMCScreen {
    FMCPage   current_page;
    bool      need_redraw;

    // 6行×2列 显示内容
    char line_L[6][24];
    char line_R[6][24];

    // 草稿栏
    char scratchpad[32];

    // EXEC灯
    bool exec_light;

    // 临时数据 (各页面共享)
    char origin[8];
    char dest[8];
    char flt_no[16];
    char crz_alt[8];
    char cost_idx[8];

    // CLB参数
    char clb_spd[8];       // 爬升速度 (250kt)
    char clb_spd_rest[16]; // 速度限制 (250/10000)
    char clb_alt_rest[8];  // 高度限制 (FL200)
    char clb_rate[8];      // 爬升率
    char clb_trans_alt[8]; // 过渡高度

    // CRZ参数
    char crz_turb_n1[8];    // 颠簸N1
    char crz_opt_alt[8];    // 最佳高度
    char crz_max_alt[8];    // 最大高度
    char crz_step[8];       // 阶梯爬升
    char crz_wind[16];      // 巡航风

    // DES参数
    char des_spd[8];       // 下降速度
    char des_spd_rest[16]; // 速度限制
    char des_alt_rest[8];  // 高度限制
    char des_rate[8];      // 下降率
    char des_trans_lvl[8]; // 过渡高度层

    FMCScreen() : current_page(PAGE_INIT_REF), need_redraw(true), exec_light(false) { clear_all(); }

    void clear_all() {
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

        // 默认值 (Boeing 737 典型值)
        strcpy(crz_alt, "FL350"); strcpy(cost_idx, "80");
        strcpy(clb_spd, "290");   strcpy(clb_spd_rest, "250/10000");
        strcpy(clb_trans_alt, "18000");
        strcpy(crz_turb_n1, "AUTO"); strcpy(crz_opt_alt, "FL345");
        strcpy(crz_max_alt, "FL390"); strcpy(crz_step, "FL370");
        strcpy(des_spd, "280");   strcpy(des_spd_rest, "240/10000");
        strcpy(des_trans_lvl, "FL180");
    }

    void clear_lines() {
        memset(line_L, 0, sizeof(line_L));
        memset(line_R, 0, sizeof(line_R));
    }

    void set_line_L(int row, const char* text) {
        if (row>=0 && row<6) strncpy(line_L[row], text, 23);
    }
    void set_line_R(int row, const char* text) {
        if (row>=0 && row<6) strncpy(line_R[row], text, 23);
    }

    // LSK: scratchpad → line, or line → scratchpad
    void lsk_press(int row, bool left) {
        char* line = left ? line_L[row] : line_R[row];
        if (scratchpad[0]) {
            strncpy(line, scratchpad, 23);
            scratchpad[0] = '\0';
        } else if (line[0]) {
            strncpy(scratchpad, line, 23);
        }
    }

    void type_char(char ch) {
        int len = strlen(scratchpad);
        if (len < 23) { scratchpad[len]=ch; scratchpad[len+1]='\0'; }
    }
    void backspace() {
        int len = strlen(scratchpad);
        if (len > 0) scratchpad[len-1]='\0';
    }
    void clear_scratchpad() { scratchpad[0]='\0'; }
};

extern FMCScreen g_screen;
extern PageDef   g_pages[];

// ===== 各页面绘制函数声明 =====
void page_draw_init_ref(FMCScreen* scr);
void page_draw_rte(FMCScreen* scr);
void page_draw_clb(FMCScreen* scr);
void page_draw_crz(FMCScreen* scr);
void page_draw_des(FMCScreen* scr);
void page_draw_dep_arr(FMCScreen* scr);
void page_draw_legs(FMCScreen* scr);
void page_draw_hold(FMCScreen* scr);
void page_draw_prog(FMCScreen* scr);

// ===== 页面切换 =====
void fmc_switch_page(FMCPage page);

// ===== 统一渲染入口 =====
void fmc_draw_screen(FMCRenderer& r);
