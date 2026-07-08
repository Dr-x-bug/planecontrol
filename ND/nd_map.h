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

// ===== 1. Compass Rose (罗盘玫瑰) =====
inline void draw_compass_rose(NDRenderer& r, double hdg) {
    int cx = ND_CX, cy = ND_CY, rad = ND_RADIUS;

    // 外圈 (粗)
    circleRGBA(r.sdl_rend, cx, cy, rad, Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, 100);
    circleRGBA(r.sdl_rend, cx, cy, rad - 1, Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, 80);

    // 半距圈 (虚线效果: 灰色细圈)
    circleRGBA(r.sdl_rend, cx, cy, rad / 2, Color::GRAY.r, Color::GRAY.g, Color::GRAY.b, 80);
    circleRGBA(r.sdl_rend, cx, cy, rad / 4, Color::GRAY.r, Color::GRAY.g, Color::GRAY.b, 50);

    // 刻度线和数字
    for (int deg = 0; deg < 360; deg += 5) {
        double a = (deg - hdg + 90.0) * M_PI / 180.0;
        double cos_a = cos(a), sin_a = sin(a);

        // 粗刻度 (每10度)
        if (deg % 10 == 0) {
            int len = 14;
            int x1 = cx + (int)((rad - 2) * cos_a);
            int y1 = cy - (int)((rad - 2) * sin_a);
            int x2 = cx + (int)((rad - 2 - len) * cos_a);
            int y2 = cy - (int)((rad - 2 - len) * sin_a);
            lineRGBA(r.sdl_rend, x1, y1, x2, y2,
                Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, 220);

            // 数字 (每30度)
            if (deg % 30 == 0) {
                int lbl = deg / 10;
                if (lbl == 0) lbl = 36;  // 360°
                char buf[8];
                snprintf(buf, sizeof(buf), "%02d", lbl);
                int tx = cx + (int)((rad - 28) * cos_a) - 10;
                int ty = cy - (int)((rad - 28) * sin_a) - 8;
                r.draw_text(tx, ty, buf, Color::WHITE, true);
            }
        }
        // 细刻度 (每5度, 非10度整数倍)
        else {
            int len = 7;
            int x1 = cx + (int)((rad - 2) * cos_a);
            int y1 = cy - (int)((rad - 2) * sin_a);
            int x2 = cx + (int)((rad - 2 - len) * cos_a);
            int y2 = cy - (int)((rad - 2 - len) * sin_a);
            lineRGBA(r.sdl_rend, x1, y1, x2, y2,
                Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, 120);
        }
    }

    // 北向箭头 (白色三角)
    double na = (0.0 - hdg + 90.0) * M_PI / 180.0;
    int nx = cx + (int)((rad + 10) * cos(na));
    int ny = cy - (int)((rad + 10) * sin(na));
    Sint16 tri_x[3] = {(Sint16)nx, (Sint16)(nx - 8), (Sint16)(nx + 8)};
    Sint16 tri_y[3] = {(Sint16)(ny - 10), (Sint16)(ny + 6), (Sint16)(ny + 6)};
    filledPolygonRGBA(r.sdl_rend, tri_x, tri_y, 3, 255, 255, 255, 255);
}

// ===== 2. Route Line (航路绘制) =====
inline void draw_route(NDRenderer& r, const std::vector<Waypoint>& wpts,
                       double ac_lat, double ac_lon, double ac_hdg) {
    if (wpts.empty()) return;

    std::vector<int> sx_list, sy_list;
    for (const auto& wp : wpts) {
        int sx, sy;
        latlon_to_xy(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, sx, sy);
        sx_list.push_back(sx);
        sy_list.push_back(sy);
    }

    // 航线阴影 (紫色偏移, 增加立体感)
    for (size_t i = 0; i < sx_list.size() - 1; i++) {
        thickLineRGBA(r.sdl_rend, sx_list[i] + 2, sy_list[i] + 2,
            sx_list[i+1] + 2, sy_list[i+1] + 2, 4,
            Color::PURPLE.r, Color::PURPLE.g, Color::PURPLE.b, 100);
    }

    // 航线主线 (绿色粗线)
    for (size_t i = 0; i < sx_list.size() - 1; i++) {
        thickLineRGBA(r.sdl_rend, sx_list[i], sy_list[i],
            sx_list[i+1], sy_list[i+1], 3,
            Color::GREEN_RTE.r, Color::GREEN_RTE.g, Color::GREEN_RTE.b, 220);
        thickLineRGBA(r.sdl_rend, sx_list[i], sy_list[i],
            sx_list[i+1], sy_list[i+1], 1,
            Color::GREEN_LT.r, Color::GREEN_LT.g, Color::GREEN_LT.b, 255);
    }
}

// ===== 3. Waypoint Markers (航路点标识) =====
inline void draw_waypoints(NDRenderer& r, const std::vector<Waypoint>& wpts,
                           double ac_lat, double ac_lon, double ac_hdg) {
    for (size_t idx = 0; idx < wpts.size(); idx++) {
        const auto& wp = wpts[idx];
        int sx, sy;
        latlon_to_xy(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, sx, sy);

        // 仅绘制显示范围内的点
        int dx = sx - ND_CX, dy = sy - ND_CY;
        if (dx*dx + dy*dy > ND_RADIUS * ND_RADIUS) continue;

        // 菱形标记 (品红色)
        int d = 6;
        Sint16 vx[4] = {(Sint16)(sx), (Sint16)(sx + d), (Sint16)(sx), (Sint16)(sx - d)};
        Sint16 vy[4] = {(Sint16)(sy - d), (Sint16)(sy), (Sint16)(sy + d), (Sint16)(sy)};
        filledPolygonRGBA(r.sdl_rend, vx, vy, 4,
            Color::MAGENTA_WP.r, Color::MAGENTA_WP.g, Color::MAGENTA_WP.b, 255);
        aapolygonRGBA(r.sdl_rend, vx, vy, 4,
            Color::MAGENTA_WP.r, Color::MAGENTA_WP.g, Color::MAGENTA_WP.b, 255);

        // 起点/终点特殊标记 (机场用方形)
        if (idx == 0 || idx == wpts.size() - 1) {
            int s2 = 8;
            rectangleRGBA(r.sdl_rend, sx - s2, sy - s2, sx + s2, sy + s2,
                255, 255, 255, 220);
        }

        // 航路点标签
        r.draw_text(sx + 8, sy - 8, wp.id, Color::MAGENTA_WP, true);
    }
}

// ===== 4. Aircraft Symbol (本机位置) =====
inline void draw_aircraft(NDRenderer& r) {
    int cx = ND_CX, cy = ND_CY;

    // 白色机身三角 (指向当前航向)
    int sz = 16;
    filledTrigonRGBA(r.sdl_rend,
        cx, cy - sz,           // 机头
        cx - sz/2, cy + sz/2,  // 左翼
        cx + sz/2, cy + sz/2,  // 右翼
        255, 255, 255, 255);

    // 白色轮廓
    aatrigonRGBA(r.sdl_rend,
        cx, cy - sz,
        cx - sz/2, cy + sz/2,
        cx + sz/2, cy + sz/2,
        255, 255, 255, 255);

    // 中心蓝点
    filledCircleRGBA(r.sdl_rend, cx, cy, 4,
        Color::BLUE_AC.r, Color::BLUE_AC.g, Color::BLUE_AC.b, 255);
    circleRGBA(r.sdl_rend, cx, cy, 5,
        255, 255, 255, 150);
}

// ===== 5. Speed Display (速度及风速显示) =====
inline void draw_speed_info(NDRenderer& r, double gs, double tas, double wind_dir, double wind_spd) {
    char buf[64];

    // 左上角: GS (地速) 大字
    snprintf(buf, sizeof(buf), "GS  %.0f kt", gs);
    r.draw_text(12, 10, buf, Color::WHITE, false);

    // TAS (真空速) 小字
    snprintf(buf, sizeof(buf), "TAS %.0f kt", tas);
    r.draw_text(12, 32, buf, Color::GREEN_LT, true);

    // 右上角: 风箭头 + 风向/风速
    int wx = ND_WIN_W - 155, wy = 28;
    double w_rad = (wind_dir - 180.0) * M_PI / 180.0;
    int arr_len = 35;

    // 风箭头杆
    int ax1 = wx - (int)(arr_len/2 * cos(w_rad));
    int ay1 = wy + (int)(arr_len/2 * sin(w_rad));
    int ax2 = wx + (int)(arr_len/2 * cos(w_rad));
    int ay2 = wy - (int)(arr_len/2 * sin(w_rad));
    lineRGBA(r.sdl_rend, ax1, ay1, ax2, ay2,
        Color::CYAN.r, Color::CYAN.g, Color::CYAN.b, 220);

    // 箭头头部
    int head_len = 10;
    double head_angle = 0.5;
    int hx1 = ax2 - (int)(head_len * cos(w_rad - head_angle));
    int hy1 = ay2 + (int)(head_len * sin(w_rad - head_angle));
    int hx2 = ax2 - (int)(head_len * cos(w_rad + head_angle));
    int hy2 = ay2 + (int)(head_len * sin(w_rad + head_angle));
    lineRGBA(r.sdl_rend, ax2, ay2, hx1, hy1, Color::CYAN.r, Color::CYAN.g, Color::CYAN.b, 220);
    lineRGBA(r.sdl_rend, ax2, ay2, hx2, hy2, Color::CYAN.r, Color::CYAN.g, Color::CYAN.b, 220);

    // 风向/风速 文字
    snprintf(buf, sizeof(buf), "%.0f°/%.0fkt", wind_dir, wind_spd);
    r.draw_text(ND_WIN_W - 195, 8, buf, Color::CYAN, true);

    // 左下角: HDG (航向)
    snprintf(buf, sizeof(buf), "HDG %.0f", g_nd.heading);
    r.draw_text(12, ND_WIN_H - 28, buf, Color::WHITE, true);

    // 右下角: 范围
    snprintf(buf, sizeof(buf), "RNG 20 NM");
    r.draw_text(ND_WIN_W - 85, ND_WIN_H - 28, buf, Color::CYAN, true);
}

// ===== Main ND MAP render =====
inline void draw_nd_map(NDRenderer& r, const std::vector<Waypoint>& wpts,
                         GridHashTable& ht) {
    // Layer 1: 罗盘玫瑰 (背景层)
    draw_compass_rose(r, g_nd.heading);

    // Layer 2: 周边导航台 (哈希表查询)
    auto nearby = query_nearby(ht, g_nd.ac_lat, g_nd.ac_lon, 80.0);
    for (auto* np : nearby) {
        int sx, sy;
        latlon_to_xy(g_nd.ac_lat, g_nd.ac_lon, g_nd.heading,
                     np->lat, np->lon, sx, sy);
        int dx = sx - ND_CX, dy = sy - ND_CY;
        if (dx*dx + dy*dy > ND_RADIUS * ND_RADIUS) continue;

        if (np->type == NAV_AIRPORT) {
            // 机场: 白色圆形 + ICAO标签
            filledCircleRGBA(r.sdl_rend, sx, sy, 6, 255, 255, 255, 220);
            circleRGBA(r.sdl_rend, sx, sy, 7, 255, 255, 255, 150);
            r.draw_text(sx + 10, sy - 5, np->id, Color::WHITE, true);
        }
        else if (np->type == NAV_VOR) {
            // VOR: 绿色小菱形
            int d = 4;
            Sint16 vx[4] = {(Sint16)(sx), (Sint16)(sx+d), (Sint16)(sx), (Sint16)(sx-d)};
            Sint16 vy[4] = {(Sint16)(sy-d), (Sint16)(sy), (Sint16)(sy+d), (Sint16)(sy)};
            filledPolygonRGBA(r.sdl_rend, vx, vy, 4,
                Color::GREEN_LT.r, Color::GREEN_LT.g, Color::GREEN_LT.b, 200);
            r.draw_text(sx + 8, sy - 5, np->id, Color::GREEN_LT, true);
        }
        else if (np->type == NAV_NDB) {
            // NDB: 青色小圆点
            filledCircleRGBA(r.sdl_rend, sx, sy, 3, Color::CYAN.r, Color::CYAN.g, Color::CYAN.b, 200);
        }
        else if (np->type == NAV_FIX) {
            // FIX: 灰色三角
            filledTrigonRGBA(r.sdl_rend, sx, sy-4, sx-4, sy+3, sx+4, sy+3,
                120, 120, 120, 180);
        }
    }

    // Layer 3: 航线 (在罗盘上面, 航路点下面)
    draw_route(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);

    // Layer 4: 航路点标记 (航线上面)
    draw_waypoints(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);

    // Layer 5: 本机符号 (最上层)
    draw_aircraft(r);

    // Layer 6: 速度/风/信息叠加层
    draw_speed_info(r, g_nd.gs, g_nd.tas, g_nd.wind_dir, g_nd.wind_spd);
}
