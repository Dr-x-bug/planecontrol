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

// ===== 工具函数: 经纬度→屏幕坐标 =====
inline double calc_bearing_deg(double lat1, double lon1, double lat2, double lon2) {
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double rlat1 = lat1 * M_PI / 180.0;
    double rlat2 = lat2 * M_PI / 180.0;
    double y = sin(dLon) * cos(rlat2);
    double x = cos(rlat1) * sin(rlat2) - sin(rlat1) * cos(rlat2) * cos(dLon);
    double brng = atan2(y, x) * 180.0 / M_PI;
    if (brng < 0) brng += 360.0;
    return brng;
}

inline double calc_dist_nm(double lat1, double lon1, double lat2, double lon2) {
    double R = 3440.065; // 地球半径 (海里)
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1 * M_PI/180.0) * cos(lat2 * M_PI/180.0) *
               sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * c;
}

// ===== 1. 罗盘玫瑰 =====
inline void draw_compass_rose(NDRenderer& r, double hdg) {
    int cx = ND_CX, cy = ND_CY, rad = ND_RADIUS;

    // 外圈
    circleRGBA(r.sdl_rend, cx, cy, rad, 180, 180, 180, 120);
    circleRGBA(r.sdl_rend, cx, cy, rad + 1, 180, 180, 180, 80);

    // 半距圈
    circleRGBA(r.sdl_rend, cx, cy, rad / 2, 80, 80, 80, 80);

    // 刻度线（每5°）
    for (int deg = 0; deg < 360; deg += 5) {
        double a = (deg - hdg) * M_PI / 180.0;  // 旋转使当前航向朝上
        double cos_a = cos(a), sin_a = sin(a);

        bool is_major = (deg % 10 == 0);
        int len = is_major ? 14 : 7;
        int alpha = is_major ? 220 : 140;

        int x1 = cx + (int)((rad - 2) * sin_a);
        int y1 = cy - (int)((rad - 2) * cos_a);
        int x2 = cx + (int)((rad - 2 - len) * sin_a);
        int y2 = cy - (int)((rad - 2 - len) * cos_a);
        lineRGBA(r.sdl_rend, x1, y1, x2, y2, 255, 255, 255, alpha);

        // 数字 (每30°)
        if (deg % 30 == 0) {
            int lbl_val = deg / 10;
            if (lbl_val == 0) lbl_val = 36; // 36 = 360°
            char buf[8];
            snprintf(buf, sizeof(buf), "%02d", lbl_val);
            int tx = cx + (int)((rad - 28) * sin_a) - 10;
            int ty = cy - (int)((rad - 28) * cos_a) - 8;
            r.draw_text(tx, ty, buf, Color::WHITE, true);
        }
    }

    // 北向箭头 (白色三角，朝向北 = 360°)
    double na = (0.0 - hdg) * M_PI / 180.0;
    int nx = cx + (int)((rad + 10) * sin(na));
    int ny = cy - (int)((rad + 10) * cos(na));
    Sint16 tri_x[3] = {(Sint16)nx, (Sint16)(nx - 8), (Sint16)(nx + 8)};
    Sint16 tri_y[3] = {(Sint16)(ny - 10), (Sint16)(ny + 6), (Sint16)(ny + 6)};
    filledPolygonRGBA(r.sdl_rend, tri_x, tri_y, 3, 255, 255, 255, 255);
}

// ===== 2. 航线绘制 (绿色粗线 + 紫色阴影) =====
inline void draw_route(NDRenderer& r, const std::vector<Waypoint>& wpts,
                       double ac_lat, double ac_lon, double ac_hdg) {
    if (wpts.size() < 2) return;

    std::vector<int> sx, sy;
    for (const auto& wp : wpts) {
        int px, py;
        latlon_to_xy(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, px, py);
        sx.push_back(px); sy.push_back(py);
    }

    // 紫色阴影
    for (size_t i = 0; i < sx.size() - 1; i++) {
        thickLineRGBA(r.sdl_rend,
            sx[i] + 2, sy[i] + 2, sx[i+1] + 2, sy[i+1] + 2,
            4, Color::PURPLE.r, Color::PURPLE.g, Color::PURPLE.b, 90);
    }
    // 绿色主线
    for (size_t i = 0; i < sx.size() - 1; i++) {
        thickLineRGBA(r.sdl_rend,
            sx[i], sy[i], sx[i+1], sy[i+1],
            3, Color::GREEN_RTE.r, Color::GREEN_RTE.g, Color::GREEN_RTE.b, 230);
        thickLineRGBA(r.sdl_rend,
            sx[i], sy[i], sx[i+1], sy[i+1],
            1, Color::GREEN_LT.r, Color::GREEN_LT.g, Color::GREEN_LT.b, 255);
    }
}

// ===== 3. 航路点标记 (品红菱形) =====
inline void draw_waypoints(NDRenderer& r, const std::vector<Waypoint>& wpts,
                           double ac_lat, double ac_lon, double ac_hdg) {
    for (size_t idx = 0; idx < wpts.size(); idx++) {
        const auto& wp = wpts[idx];
        int sx, sy;
        latlon_to_xy(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, sx, sy);

        int dx = sx - ND_CX, dy = sy - ND_CY;
        if (dx*dx + dy*dy > ND_RADIUS * ND_RADIUS) continue;

        // 菱形
        int d = 6;
        Sint16 vx[4] = {(Sint16)sx, (Sint16)(sx+d), (Sint16)sx, (Sint16)(sx-d)};
        Sint16 vy[4] = {(Sint16)(sy-d), (Sint16)sy, (Sint16)(sy+d), (Sint16)sy};
        filledPolygonRGBA(r.sdl_rend, vx, vy, 4,
            Color::MAGENTA_WP.r, Color::MAGENTA_WP.g, Color::MAGENTA_WP.b, 255);
        aapolygonRGBA(r.sdl_rend, vx, vy, 4,
            Color::MAGENTA_WP.r, Color::MAGENTA_WP.g, Color::MAGENTA_WP.b, 255);

        // 起点/终点白色方框
        if (idx == 0 || idx == wpts.size() - 1) {
            int s2 = 7;
            rectangleRGBA(r.sdl_rend, sx - s2, sy - s2, sx + s2, sy + s2, 255, 255, 255, 220);
        }

        // 标签
        r.draw_text(sx + 8, sy - 8, wp.id, Color::MAGENTA_WP, true);
    }
}

// ===== 4. 周边导航台 (哈希表查询) =====
inline void draw_navaids(NDRenderer& r, GridHashTable& ht,
                         double ac_lat, double ac_lon, double ac_hdg) {
    auto nearby = query_nearby(ht, ac_lat, ac_lon, 80.0);
    for (auto* np : nearby) {
        int sx, sy;
        latlon_to_xy(ac_lat, ac_lon, ac_hdg, np->lat, np->lon, sx, sy);
        int dx = sx - ND_CX, dy = sy - ND_CY;
        if (dx*dx + dy*dy > ND_RADIUS * ND_RADIUS) continue;

        if (np->type == NAV_AIRPORT) {
            // 白色圆圈 + 标签
            circleRGBA(r.sdl_rend, sx, sy, 6, 255, 255, 255, 200);
            r.draw_text(sx + 10, sy - 5, np->id, Color::WHITE, true);
        } else if (np->type == NAV_VOR) {
            // 绿色小菱形
            int d = 4;
            Sint16 vx[4] = {(Sint16)sx, (Sint16)(sx+d), (Sint16)sx, (Sint16)(sx-d)};
            Sint16 vy[4] = {(Sint16)(sy-d), (Sint16)sy, (Sint16)(sy+d), (Sint16)sy};
            filledPolygonRGBA(r.sdl_rend, vx, vy, 4, 160, 222, 164, 200);
        } else if (np->type == NAV_NDB) {
            // 青色圆点
            filledCircleRGBA(r.sdl_rend, sx, sy, 3, 0, 200, 200, 180);
        } else if (np->type == NAV_FIX) {
            // 灰色小三角
            filledTrigonRGBA(r.sdl_rend, sx, sy - 4, sx - 4, sy + 3, sx + 4, sy + 3, 120, 120, 120, 160);
        }
    }
}

// ===== 5. 本机符号 =====
inline void draw_aircraft(NDRenderer& r) {
    int cx = ND_CX, cy = ND_CY;
    // 白色三角 (机头朝上)
    int sz = 16;
    filledTrigonRGBA(r.sdl_rend,
        cx, cy - sz,
        cx - sz/2, cy + sz/2,
        cx + sz/2, cy + sz/2,
        255, 255, 255, 255);
    // 中心蓝点
    filledCircleRGBA(r.sdl_rend, cx, cy, 4, 0, 0, 255, 255);
    circleRGBA(r.sdl_rend, cx, cy, 5, 255, 255, 255, 150);
}

// ===== 6. 速度/风/信息叠加 =====
inline void draw_info_overlay(NDRenderer& r) {
    char buf[64];

    // 左上: GS (大字) + TAS (小字)
    snprintf(buf, sizeof(buf), "GS  %.0f kt", g_nd.gs);
    r.draw_text(12, 10, buf, Color::WHITE, false);
    snprintf(buf, sizeof(buf), "TAS %.0f kt", g_nd.tas);
    r.draw_text(12, 34, buf, Color::GREEN_LT, true);

    // 右上: 风向箭头 + 风速
    int wx = ND_WIN_W - 160, wy = 28;
    double w_rad = (g_nd.wind_dir - 180.0) * M_PI / 180.0;
    int arr_len = 35;
    int ax1 = wx - (int)(arr_len/2 * cos(w_rad));
    int ay1 = wy + (int)(arr_len/2 * sin(w_rad));
    int ax2 = wx + (int)(arr_len/2 * cos(w_rad));
    int ay2 = wy - (int)(arr_len/2 * sin(w_rad));
    lineRGBA(r.sdl_rend, ax1, ay1, ax2, ay2, 0, 200, 200, 220);
    // 箭头头部
    int hl = 10; double ha = 0.5;
    int hx1 = ax2 - (int)(hl * cos(w_rad - ha));
    int hy1 = ay2 + (int)(hl * sin(w_rad - ha));
    int hx2 = ax2 - (int)(hl * cos(w_rad + ha));
    int hy2 = ay2 + (int)(hl * sin(w_rad + ha));
    lineRGBA(r.sdl_rend, ax2, ay2, hx1, hy1, 0, 200, 200, 220);
    lineRGBA(r.sdl_rend, ax2, ay2, hx2, hy2, 0, 200, 200, 220);
    snprintf(buf, sizeof(buf), "%.0f\u00b0/%.0fkt", g_nd.wind_dir, g_nd.wind_spd);
    r.draw_text(ND_WIN_W - 200, 8, buf, Color::CYAN, true);

    // 左下: HDG
    snprintf(buf, sizeof(buf), "HDG %.0f", g_nd.heading);
    r.draw_text(12, ND_WIN_H - 28, buf, Color::WHITE, true);

    // 右下: 范围
    r.draw_text(ND_WIN_W - 80, ND_WIN_H - 28, "RNG 20 NM", Color::CYAN, true);
}

// ===== 主渲染入口 =====
inline void draw_nd_map(NDRenderer& r, const std::vector<Waypoint>& wpts,
                         GridHashTable& ht) {
    // Layer 1: 罗盘
    draw_compass_rose(r, g_nd.heading);
    // Layer 2: 导航台
    draw_navaids(r, ht, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);
    // Layer 3: 航线
    draw_route(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);
    // Layer 4: 航路点
    draw_waypoints(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);
    // Layer 5: 本机
    draw_aircraft(r);
    // Layer 6: 信息覆盖层
    draw_info_overlay(r);
}
