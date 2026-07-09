#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include "config.h"
#include "renderer.h"
#include "navdata.h"
#include "nd_data.h"
#include "navaid_hash.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ===== 工具函数 =====
inline double calc_bearing_deg(double lat1, double lon1, double lat2, double lon2) {
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double rlat1 = lat1 * M_PI / 180.0, rlat2 = lat2 * M_PI / 180.0;
    double y = sin(dLon) * cos(rlat2);
    double x = cos(rlat1) * sin(rlat2) - sin(rlat1) * cos(rlat2) * cos(dLon);
    double brng = atan2(y, x) * 180.0 / M_PI;
    if (brng < 0) brng += 360.0;
    return brng;
}
inline double calc_dist_nm(double lat1, double lon1, double lat2, double lon2) {
    double R = 3440.065, dlat = (lat2-lat1)*M_PI/180.0, dlon = (lon2-lon1)*M_PI/180.0;
    double a = sin(dlat/2)*sin(dlat/2) + cos(lat1*M_PI/180.0)*cos(lat2*M_PI/180.0)*sin(dlon/2)*sin(dlon/2);
    return R * 2 * atan2(sqrt(a), sqrt(1-a));
}
inline void wpt_to_screen(double ac_lat, double ac_lon, double ac_hdg,
                          double wpt_lat, double wpt_lon, int& sx, int& sy) {
    double dist_nm = calc_dist_nm(ac_lat, ac_lon, wpt_lat, wpt_lon);
    double bearing = calc_bearing_deg(ac_lat, ac_lon, wpt_lat, wpt_lon);
    double px = dist_nm / NM_PER_PX;
    double rad = (bearing - ac_hdg) * M_PI / 180.0;
    sx = ND_CX + (int)(px * sin(rad));
    sy = ND_CY - (int)(px * cos(rad));
}

// ===== ND Arc 模式渲染参数 =====
constexpr int ARC_CX        = 375;
constexpr int ARC_CY        = 280;
constexpr int ARC_R         = 265;
constexpr int ARC_START     = 30;   // arcRGBA: 右端
constexpr int ARC_END       = 150;  // arcRGBA: 左端
constexpr int ACFT_Y        = 555;
constexpr int CTR_LINE_TOP  = 120;

// ===== L1: 罗盘弧线 + 刻度 + 数字 =====
inline void draw_arc_compass(NDRenderer& r, double hdg) {
    (void)hdg;
    int cx = ARC_CX, cy = ARC_CY, rad = ARC_R;
    arcRGBA(r.sdl_rend, cx, cy, rad, ARC_START, ARC_END, 255, 255, 255, 200);
    arcRGBA(r.sdl_rend, cx, cy, rad+1, ARC_START, ARC_END, 255, 255, 255, 120);

    for (int a = ARC_START; a <= ARC_END; a += 5) {
        double ra = a * M_PI / 180.0;
        double cosa = cos(ra), sina = sin(ra);
        bool is10 = (a % 10 == 0);
        int tl = is10 ? 16 : 8;
        int al = is10 ? 220 : 130;
        int x1 = cx + (int)((rad-1)*cosa), y1 = cy - (int)((rad-1)*sina);
        int x2 = cx + (int)((rad-1+tl)*cosa), y2 = cy - (int)((rad-1+tl)*sina);
        lineRGBA(r.sdl_rend, x1, y1, x2, y2, 255, 255, 255, al);

        // 数字: 映射 arc角度→罗盘航向
        int lbl = -1;
        if (a == 30)       lbl = 4;    // 40°
        else if (a == 40)  lbl = 3;    // 30°
        else if (a == 50)  lbl = 2;    // 20°
        else if (a == 60)  lbl = 1;    // 10°
        else if (a == 70)  lbl = 0;    // 0° (top actually = 360)
        else if (a == 80)  lbl = 0;    // 360°
        else if (a == 90)  lbl = 0;    // middle
        else if (a == 100) lbl = 35;   // 350°
        else if (a == 110) lbl = 34;   // 340°
        else if (a == 120) lbl = 33;   // 330°
        else if (a == 130) lbl = 32;   // 320°
        else if (a == 140) lbl = 31;   // 310°
        else if (a == 150) lbl = 30;   // 300°
        if (lbl >= 0) {
            char buf[4]; snprintf(buf, sizeof(buf), "%d", lbl);
            int tx = cx + (int)((rad+24)*cosa) - (lbl>=10?10:5);
            int ty = cy - (int)((rad+24)*sina) - 8;
            r.draw_text(tx, ty, buf, Color::WHITE, true);
        }
    }
    // 航向指针(弧顶三角)
    int py = cy - rad - 2;
    Sint16 tri_x[3] = {(Sint16)cx, (Sint16)(cx-6), (Sint16)(cx+6)};
    Sint16 tri_y[3] = {(Sint16)(py-10), (Sint16)py, (Sint16)py};
    filledPolygonRGBA(r.sdl_rend, tri_x, tri_y, 3, 255, 255, 255, 255);
}

// ===== L2: 垂直中心线 + 刻度 =====
inline void draw_center_line(NDRenderer& r) {
    lineRGBA(r.sdl_rend, ARC_CX, CTR_LINE_TOP, ARC_CX, ACFT_Y-25, 255, 255, 255, 100);
    int seg = (ACFT_Y-25 - CTR_LINE_TOP) / 4;
    for (int i = 1; i <= 3; i++) {
        int ty = CTR_LINE_TOP + i*seg;
        lineRGBA(r.sdl_rend, ARC_CX-8, ty, ARC_CX+8, ty, 255, 255, 255, 160);
    }
}

// ===== L3: 飞机符号 (空心三角+中心点+周边6点) =====
inline void draw_aircraft_symbol(NDRenderer& r) {
    int cx = ARC_CX, cy = ACFT_Y, sz = 12;
    int tx = cx, ty = cy - sz;
    int lx = cx - sz/2, ly = cy + sz/2;
    int rx = cx + sz/2, ry = cy + sz/2;
    lineRGBA(r.sdl_rend, tx, ty, lx, ly, 255, 255, 255, 220);
    lineRGBA(r.sdl_rend, lx, ly, rx, ry, 255, 255, 255, 220);
    lineRGBA(r.sdl_rend, rx, ry, tx, ty, 255, 255, 255, 220);
    filledCircleRGBA(r.sdl_rend, cx, cy, 4, 255, 255, 255, 255);
    // 6个小点: 半圆在三角上方
    for (int i = 0; i < 6; i++) {
        double ang = M_PI * (double)i / 5.0;
        int dx = (int)(18*cos(ang)), dy = -(int)(18*sin(ang));
        filledCircleRGBA(r.sdl_rend, cx+dx, cy+dy, 2, 200, 200, 200, 200);
    }
}

// ===== L4: 航线 =====
inline void draw_route(NDRenderer& r, const std::vector<Waypoint>& wpts,
                       double ac_lat, double ac_lon, double ac_hdg) {
    if (wpts.size() < 2) return;
    std::vector<int> sx, sy;
    for (const auto& wp : wpts) {
        int px, py;
        wpt_to_screen(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, px, py);
        sx.push_back(px); sy.push_back(py);
    }
    for (size_t i = 0; i < sx.size()-1; i++) {
        thickLineRGBA(r.sdl_rend, sx[i]+2, sy[i]+2, sx[i+1]+2, sy[i+1]+2,
            4, Color::PURPLE.r, Color::PURPLE.g, Color::PURPLE.b, 90);
    }
    for (size_t i = 0; i < sx.size()-1; i++) {
        thickLineRGBA(r.sdl_rend, sx[i], sy[i], sx[i+1], sy[i+1],
            3, Color::GREEN_RTE.r, Color::GREEN_RTE.g, Color::GREEN_RTE.b, 240);
        thickLineRGBA(r.sdl_rend, sx[i], sy[i], sx[i+1], sy[i+1],
            1, Color::GREEN_LT.r, Color::GREEN_LT.g, Color::GREEN_LT.b, 255);
    }
}

// ===== L5: 航路点 (品红菱形) =====
inline void draw_waypoints(NDRenderer& r, const std::vector<Waypoint>& wpts,
                           double ac_lat, double ac_lon, double ac_hdg) {
    for (size_t idx = 0; idx < wpts.size(); idx++) {
        const auto& wp = wpts[idx];
        double dist_nm = calc_dist_nm(ac_lat, ac_lon, wp.lat, wp.lon);
        if (dist_nm > 80.0) continue;
        double bearing = calc_bearing_deg(ac_lat, ac_lon, wp.lat, wp.lon);
        double rel = bearing - ac_hdg;
        while (rel > 180) rel -= 360;
        while (rel < -180) rel += 360;
        if (fabs(rel) > 45.0) continue;
        int sx, sy;
        wpt_to_screen(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, sx, sy);
        int dx = sx-ND_CX, dy = sy-ND_CY;
        if (dx*dx+dy*dy > ARC_R*ARC_R && sy > ARC_CY+ARC_R) continue;
        int d = 6;
        Sint16 vx[4] = {(Sint16)sx,(Sint16)(sx+d),(Sint16)sx,(Sint16)(sx-d)};
        Sint16 vy[4] = {(Sint16)(sy-d),(Sint16)sy,(Sint16)(sy+d),(Sint16)sy};
        filledPolygonRGBA(r.sdl_rend, vx, vy, 4,
            Color::MAGENTA_WP.r, Color::MAGENTA_WP.g, Color::MAGENTA_WP.b, 255);
        aapolygonRGBA(r.sdl_rend, vx, vy, 4,
            Color::MAGENTA_WP.r, Color::MAGENTA_WP.g, Color::MAGENTA_WP.b, 255);
        if (idx==0 || idx==wpts.size()-1) {
            int s2 = 7;
            rectangleRGBA(r.sdl_rend, sx-s2, sy-s2, sx+s2, sy+s2, 255, 255, 255, 220);
        }
        r.draw_text(sx+8, sy-8, wp.id, Color::MAGENTA_WP, true);
    }
}

// ===== L6: 导航台 =====
inline void draw_navaids(NDRenderer& r, GridHashTable& ht,
                         double ac_lat, double ac_lon, double ac_hdg) {
    auto nearby = query_nearby(ht, ac_lat, ac_lon, 80.0);
    for (auto* np : nearby) {
        int sx, sy;
        wpt_to_screen(ac_lat, ac_lon, ac_hdg, np->lat, np->lon, sx, sy);
        int dx = sx-ND_CX, dy = sy-ND_CY;
        if (dx*dx+dy*dy > ARC_R*ARC_R && sy > ARC_CY+ARC_R) continue;
        if (np->type == NAV_AIRPORT) {
            circleRGBA(r.sdl_rend, sx, sy, 6, 255, 255, 255, 200);
        } else if (np->type == NAV_VOR) {
            int d=4; Sint16 vx[4]={(Sint16)sx,(Sint16)(sx+d),(Sint16)sx,(Sint16)(sx-d)};
            Sint16 vy[4]={(Sint16)(sy-d),(Sint16)sy,(Sint16)(sy+d),(Sint16)sy};
            filledPolygonRGBA(r.sdl_rend,vx,vy,4,160,222,164,200);
        } else if (np->type == NAV_NDB) {
            filledCircleRGBA(r.sdl_rend,sx,sy,3,0,200,200,180);
        } else if (np->type == NAV_FIX) {
            filledTrigonRGBA(r.sdl_rend,sx,sy-4,sx-4,sy+3,sx+4,sy+3,120,120,120,160);
        }
    }
}

// ===== L7: 左上速度+风 =====
inline void draw_speed_wind(NDRenderer& r) {
    char buf[64];
    snprintf(buf, sizeof(buf), "GS  %.0f kt", g_nd.gs);
    r.draw_text(12, 10, buf, Color::WHITE, false);
    snprintf(buf, sizeof(buf), "TAS %.0f kt", g_nd.tas);
    r.draw_text(12, 36, buf, Color::GREEN_LT, true);
    r.draw_text(12, 56, "---\u00b0/---", Color::CYAN, true);
}

// ===== L8: 右上航路点 =====
inline void draw_next_wpt(NDRenderer& r) {
    int px = ND_WIN_W - 155, py = 10;
    r.draw_text(px, py, "RW02L", Color::MAGENTA_WP, false);
    r.draw_text(px, py+26, "MIN", Color::WHITE, true);
    r.draw_text(px+45, py+26, "01.0NM", Color::CYAN, true);
}

// ===== L9: 底部信息 =====
inline void draw_bottom_info(NDRenderer& r) {
    char buf[64];
    snprintf(buf, sizeof(buf), "HDG %.0f", g_nd.heading);
    r.draw_text(12, ND_WIN_H-28, buf, Color::WHITE, true);
    r.draw_text(ND_WIN_W-80, ND_WIN_H-28, "RNG 20 NM", Color::CYAN, true);
}

// ===== 主渲染 =====
inline void draw_nd_map(NDRenderer& r, const std::vector<Waypoint>& wpts,
                         GridHashTable& ht) {
    draw_arc_compass(r, g_nd.heading);
    draw_center_line(r);
    draw_navaids(r, ht, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);
    draw_route(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);
    draw_waypoints(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);
    draw_aircraft_symbol(r);
    draw_speed_wind(r);
    draw_next_wpt(r);
    draw_bottom_info(r);
}
