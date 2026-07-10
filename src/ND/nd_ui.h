/**
 * nd_ui.h — ND 模块界面层头文件
 * 职责: 声明窗口/渲染器结构、颜色常量、绘制函数、坐标工具函数
 * 依赖: SDL2 + SDL2_ttf + SDL2_gfx
 */
#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <string>
#include <vector>

// ============================================================
//  一、窗口与显示参数
// ============================================================

constexpr int ND_WIN_W = 751;
constexpr int ND_WIN_H = 721;

// ============================================================
//  二、颜色常量
// ============================================================

namespace Color {
    constexpr SDL_Color BG_BLACK   = {  0,   0,   0, 255};
    constexpr SDL_Color WHITE      = {255, 255, 255, 255};
    constexpr SDL_Color GREEN_RTE  = { 33, 132,  19, 255};
    constexpr SDL_Color GREEN_LT   = {160, 222, 164, 255};
    constexpr SDL_Color MAGENTA_WP = {223, 124, 237, 255};
    constexpr SDL_Color PURPLE     = { 96,  34,  92, 255};
    constexpr SDL_Color BLUE_AC    = {  0,   0, 255, 255};
    constexpr SDL_Color GRAY       = {100, 100, 100, 255};
    constexpr SDL_Color CYAN       = {  0, 200, 200, 255};
    constexpr SDL_Color YELLOW     = {255, 255,   0, 255};
}

// ============================================================
//  三、ND 显示参数
// ============================================================

constexpr int   ND_CX      = ND_WIN_W / 2;
constexpr int   ND_CY      = ND_WIN_H / 2;
constexpr int   ND_RADIUS  = 300;
constexpr double NM_PER_PX = 0.0667;

// Arc 模式渲染参数
constexpr int ARC_CX       = 375;
constexpr int ARC_CY       = 480;
constexpr int ARC_R        = 390;
constexpr int ARC_START    = 210;
constexpr int ARC_END      = 330;
constexpr int ACFT_Y       = 555;

// ============================================================
//  四、ND 渲染器结构
// ============================================================

struct NDRenderer {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* sdl_rend = nullptr;
    TTF_Font*     font     = nullptr;
    TTF_Font*     font_sm  = nullptr;

    bool init();
    void clear();
    void present();
    void set_color(SDL_Color c);

    void draw_text(int x, int y, const std::string& txt, SDL_Color c, bool small = false);
    void draw_text_center(int cx, int y, const std::string& txt, SDL_Color c, bool small = false);
    void draw_text_right(int rx, int y, const std::string& txt, SDL_Color c, bool small = false);

    ~NDRenderer();
};

// ============================================================
//  五、工具函数声明
// ============================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double normalize_angle(double angle);
double calc_bearing_deg(double lat1, double lon1, double lat2, double lon2);
double calc_dist_nm(double lat1, double lon1, double lat2, double lon2);
void   wpt_to_screen(double ac_lat, double ac_lon, double ac_hdg,
                     double wpt_lat, double wpt_lon, int& sx, int& sy);
int    arc_heading_label(double hdg, int arc_angle);

// ============================================================
//  六、ND 绘制函数声明
// ============================================================

struct Waypoint;
struct GridHashTable;

void draw_top_mode(NDRenderer& r);
void draw_speed_wind(NDRenderer& r);
void draw_next_wpt(NDRenderer& r);
void draw_arc_compass(NDRenderer& r, double hdg);
void draw_aircraft_symbol(NDRenderer& r);
void draw_center_line(NDRenderer& r);
void draw_route(NDRenderer& r, const std::vector<Waypoint>& wpts,
                double ac_lat, double ac_lon, double ac_hdg);
void draw_waypoints(NDRenderer& r, const std::vector<Waypoint>& wpts,
                    double ac_lat, double ac_lon, double ac_hdg);
void draw_navaids(NDRenderer& r, GridHashTable& ht,
                  double ac_lat, double ac_lon, double ac_hdg);
void draw_bottom_info(NDRenderer& r);

// 主渲染入口
void draw_nd_map(NDRenderer& r, const std::vector<Waypoint>& wpts,
                 GridHashTable& ht);
