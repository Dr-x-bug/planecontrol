#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include "config.h"
#include "renderer.h"
#include "navdata.h"
#include "nd_data.h"
#include "navaid_hash.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =====================================================================
//  辅助数学函数
// =====================================================================

inline double normalize_angle(double angle) {
    double v = fmod(angle, 360.0);
    if (v < 0) v += 360.0;
    return v;
}

inline double calc_bearing_deg(double lat1, double lon1, double lat2, double lon2) {
    double dLon = lon2 - lon1;
    while (dLon > 180.0) dLon -= 360.0;
    while (dLon < -180.0) dLon += 360.0;
    dLon *= M_PI / 180.0;
    double rlat1 = lat1 * M_PI / 180.0, rlat2 = lat2 * M_PI / 180.0;
    double y = sin(dLon) * cos(rlat2);
    double x = cos(rlat1) * sin(rlat2) - sin(rlat1) * cos(rlat2) * cos(dLon);
    double brng = atan2(y, x) * 180.0 / M_PI;
    if (brng < 0) brng += 360.0;
    return brng;
}

inline double calc_dist_nm(double lat1, double lon1, double lat2, double lon2) {
    double R = 3440.065;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat/2)*sin(dlat/2) +
               cos(lat1*M_PI/180.0)*cos(lat2*M_PI/180.0)*sin(dlon/2)*sin(dlon/2);
    return R * 2 * atan2(sqrt(a), sqrt(1-a));
}

// =====================================================================
//  常量定义 (ND_WIN_W/H 和 NM_PER_PX 已在 config.h 中定义)
// =====================================================================

// 罗盘弧线几何 (向上凸起的拱形)
// 弧线中心在 (375, 300), 半径 270px
// 显示相对方位 +/-40deg (共 80deg 视野)
constexpr int ARC_CX        = 375;
constexpr int ARC_CY        = 300;
constexpr int ARC_R         = 270;
constexpr int ARC_HALF_SWEEP = 40;   // 相对方位半角 (度)

// 飞机符号位置
constexpr int ACFT_X        = 375;
constexpr int ACFT_Y        = 555;

// =====================================================================
//  角度转换: 相对方位角 <-> SDL2_gfx 数学角度
// =====================================================================
// SDL2_gfx arcRGBA 使用数学角度: 0deg=正右(East), 逆时针增加
// 导航相对方位: 0deg=正北(North), 顺时针增加
// 转换: sdl_angle = 90.0 - nav_angle
//       nav_angle = 90.0 - sdl_angle
//
// 相对方位 rel_deg: -40(左) ~ +40(右), 0=正上方
// nav_angle = (360 + rel_deg) % 360
// sdl_angle = 90 - nav_angle

inline double rel_to_sdl_angle(double rel_deg) {
    double nav = fmod(360.0 + rel_deg, 360.0);
    if (nav < 0) nav += 360.0;
    return 90.0 - nav;
}

// =====================================================================
//  坐标变换: 地理坐标 -> 屏幕坐标
// =====================================================================

inline void wpt_to_screen(double ac_lat, double ac_lon, double ac_hdg,
                          double wpt_lat, double wpt_lon, int& sx, int& sy) {
    double dist_nm = calc_dist_nm(ac_lat, ac_lon, wpt_lat, wpt_lon);
    double bearing = calc_bearing_deg(ac_lat, ac_lon, wpt_lat, wpt_lon);
    double rel = bearing - ac_hdg;
    // 归一化到 [-180, 180]
    while (rel > 180.0) rel -= 360.0;
    while (rel < -180.0) rel += 360.0;

    double px = dist_nm / NM_PER_PX;
    double rad = rel * M_PI / 180.0;
    sx = ARC_CX + (int)(px * sin(rad));
    sy = ARC_CY - (int)(px * cos(rad));
}

// =====================================================================
//  绘制函数
// =====================================================================

// ---- 顶部状态栏: 航向框 + TRK/MAG + 倒三角 ----
inline void draw_top_mode(NDRenderer& r) {
    int hdg = (int)round(normalize_angle(g_nd.heading));
    char buf[16];
    snprintf(buf, sizeof(buf), "%03d", hdg);

    // 航向框: 白色矩形
    int box_w = 40, box_h = 24, box_y = 4;
    int box_x = ARC_CX - box_w / 2;
    rectangleRGBA(r.sdl_rend, box_x, box_y, box_x + box_w, box_y + box_h,
                  255, 255, 255, 255);
    r.draw_text_center(ARC_CX, box_y + 4, buf, Color::WHITE, false);

    // TRK / MAG 标签
    r.draw_text(box_x - 50, box_y + 4, "TRK", Color::GREEN_RTE, true);
    r.draw_text(box_x + box_w + 8, box_y + 4, "MAG", Color::GREEN_RTE, true);

    // 倒三角指针 (box 下方, 指向弧线 0deg)
    int tri_y = box_y + box_h + 2;
    Sint16 vx[3] = {(Sint16)ARC_CX, (Sint16)(ARC_CX - 7), (Sint16)(ARC_CX + 7)};
    Sint16 vy[3] = {(Sint16)(tri_y + 10), (Sint16)tri_y, (Sint16)tri_y};
    filledPolygonRGBA(r.sdl_rend, vx, vy, 3, 255, 255, 255, 255);
}

// ---- 左上角: GS / TAS / 风向 ----
inline void draw_speed_wind(NDRenderer& r) {
    char buf[64];
    snprintf(buf, sizeof(buf), "GS: %.0f TAS: %.0f", g_nd.gs, g_nd.tas);
    r.draw_text(12, 10, buf, Color::WHITE, false);
    r.draw_text(12, 34, "---°/---", Color::CYAN, true);
}

// ---- 右上角: 跑道 / MIN / 距离 ----
inline void draw_next_wpt(NDRenderer& r) {
    int rx = ND_WIN_W - 12;
    r.draw_text_right(rx, 10, "RW02L", Color::MAGENTA_WP, false);
    r.draw_text_right(rx, 36, "MIN", Color::WHITE, true);
    r.draw_text_right(rx, 58, "01.0NM", Color::CYAN, true);
}

// ---- 罗盘弧线 + 刻度线 + 航向标签 ----
inline void draw_arc_compass(NDRenderer& r, double /*hdg*/) {
    int cx = ARC_CX, cy = ARC_CY, rad = ARC_R;

    // 绘制弧线本体 (双线形成粗弧线效果)
    // 相对方位从 -40deg 到 +40deg, 转为 SDL 角度:
    //   rel=-40 → nav=320 → sdl=130° (左上)
    //   rel=+40 → nav=40  → sdl=50°  (右上)
    // SDL2_gfx arcRGBA 以逆时针方向从 start 画到 end
    // 要画上方的拱形弧，需要从 50°(右上) CCW 到 130°(左上)，经过 90°(正上)
    // 注意: start 必须小于 end，否则走 280° 的远路画到下方去
    double sdl_right = rel_to_sdl_angle((double)ARC_HALF_SWEEP);    // 50° (较小值, 作为 start)
    double sdl_left  = rel_to_sdl_angle(-(double)ARC_HALF_SWEEP);   // 130° (较大值, 作为 end)

    arcRGBA(r.sdl_rend, cx, cy, rad, sdl_right, sdl_left,
            255, 255, 255, 220);
    arcRGBA(r.sdl_rend, cx, cy, rad + 1, sdl_right, sdl_left,
            255, 255, 255, 140);

    // 绘制刻度线和标签
    for (int rel = -(int)ARC_HALF_SWEEP; rel <= (int)ARC_HALF_SWEEP; rel += 5) {
        double sdl_a = rel_to_sdl_angle((double)rel);
        double sdl_rad = sdl_a * M_PI / 180.0;
        double cosa = cos(sdl_rad);
        double sina = sin(sdl_rad);

        bool major = (rel % 10 == 0);
        int tickLen = major ? 15 : 7;
        int alpha   = major ? 220 : 130;

        // 刻度线: 从弧线向外辐射 (SDL 坐标系中, 上凸弧的外侧 = y 减小方向)
        int x1 = cx + (int)((rad - 1) * cosa);
        int y1 = cy - (int)((rad - 1) * sina);
        int x2 = cx + (int)((rad - 1 + tickLen) * cosa);
        int y2 = cy - (int)((rad - 1 + tickLen) * sina);
        lineRGBA(r.sdl_rend, x1, y1, x2, y2,
                 255, 255, 255, alpha);

        // 标签 (仅在 major 刻度处)
        if (major) {
            // 相对方位 rel: -40 ~ +40
            // 导航角度 nav = (360 + rel) % 360
            int nav = (int)fmod(360.0 + rel, 360.0);
            // 标签显示: nav / 10 (去掉个位)
            int label = nav / 10;

            // 标签位置: 刻度线外侧 (远离弧线中心 = y 减小方向)
            int tx = cx + (int)((rad + 22) * cosa);
            int ty = cy - (int)((rad + 22) * sina);

            char buf[4];
            snprintf(buf, sizeof(buf), "%d", label);
            // 水平居中
            int tw = (label >= 10) ? 10 : 5;
            r.draw_text(tx - tw, ty - 8, buf, Color::WHITE, true);
        }
    }

    // 航向指针三角形 (弧线顶部, 指向弧线)
    // 在 SDL 坐标系中, 0deg (nav) = 90deg (sdl), cosa=0, sina=1
    // 所以三角形在 (cx, cy - rad) 即弧线最上方
    int py = cy - rad;  // 弧线顶端 Y
    Sint16 tri_x[3] = {(Sint16)cx, (Sint16)(cx - 6), (Sint16)(cx + 6)};
    Sint16 tri_y[3] = {(Sint16)(py - 8), (Sint16)(py + 2), (Sint16)(py + 2)};
    filledPolygonRGBA(r.sdl_rend, tri_x, tri_y, 3, 255, 255, 255, 255);
}

// ---- 中心航迹线 (飞机 -> 弧线顶部) ----
inline void draw_center_line(NDRenderer& r) {
    int top = ARC_CY - ARC_R;
    lineRGBA(r.sdl_rend, ARC_CX, top, ARC_CX, ACFT_Y - 16,
             255, 255, 255, 100);
}

// ---- 飞机符号 (空心三角 + 中心圆点 + 8方位点) ----
inline void draw_aircraft_symbol(NDRenderer& r) {
    int cx = ARC_CX, cy = ACFT_Y, sz = 14;

    // 空心白色三角形 (尖端朝上)
    Sint16 vx[3] = {(Sint16)cx, (Sint16)(cx - sz), (Sint16)(cx + sz)};
    Sint16 vy[3] = {(Sint16)(cy - sz), (Sint16)(cy + sz / 2), (Sint16)(cy + sz / 2)};
    aapolygonRGBA(r.sdl_rend, vx, vy, 3, 255, 255, 255, 255);

    // 中心实心圆点
    filledCircleRGBA(r.sdl_rend, cx, cy, 4, 255, 255, 255, 255);

    // 8 个方位参考点 (每 45deg)
    int rdot = 20;
    for (int i = 0; i < 8; i++) {
        double ang = i * M_PI / 4.0;
        int dx = (int)(rdot * cos(ang));
        int dy = (int)(rdot * sin(ang));
        filledCircleRGBA(r.sdl_rend, cx + dx, cy - dy, 2,
                         200, 200, 200, 200);
    }

    // 从三角形顶部向上延伸到弧线的航迹线
    lineRGBA(r.sdl_rend, cx, cy - sz, cx, ARC_CY - ARC_R,
             255, 255, 255, 120);
}

// ---- 航路线 (绿色主线 + 紫色阴影) ----
inline void draw_route(NDRenderer& r, const std::vector<Waypoint>& wpts,
                       double ac_lat, double ac_lon, double ac_hdg) {
    if (wpts.size() < 2) return;

    std::vector<int> sx, sy;
    sx.reserve(wpts.size());
    sy.reserve(wpts.size());
    for (const auto& wp : wpts) {
        int px, py;
        wpt_to_screen(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, px, py);
        sx.push_back(px);
        sy.push_back(py);
    }

    // 紫色阴影 (偏移 +2,+2)
    for (size_t i = 0; i < sx.size() - 1; i++) {
        thickLineRGBA(r.sdl_rend, sx[i] + 2, sy[i] + 2,
                      sx[i + 1] + 2, sy[i + 1] + 2,
                      4, Color::PURPLE.r, Color::PURPLE.g, Color::PURPLE.b, 90);
    }

    // 绿色主线 (粗 + 细两层)
    for (size_t i = 0; i < sx.size() - 1; i++) {
        thickLineRGBA(r.sdl_rend, sx[i], sy[i],
                      sx[i + 1], sy[i + 1],
                      3, Color::GREEN_RTE.r, Color::GREEN_RTE.g, Color::GREEN_RTE.b, 240);
        thickLineRGBA(r.sdl_rend, sx[i], sy[i],
                      sx[i + 1], sy[i + 1],
                      1, Color::GREEN_LT.r, Color::GREEN_LT.g, Color::GREEN_LT.b, 255);
    }
}

// ---- 航路点标记 (蓝色向下三角形 + 白色标签) ----
inline void draw_waypoints(NDRenderer& r, const std::vector<Waypoint>& wpts,
                           double ac_lat, double ac_lon, double ac_hdg) {
    for (size_t idx = 0; idx < wpts.size(); idx++) {
        const auto& wp = wpts[idx];

        double dist_nm = calc_dist_nm(ac_lat, ac_lon, wp.lat, wp.lon);
        if (dist_nm > 80.0) continue;

        double bearing = calc_bearing_deg(ac_lat, ac_lon, wp.lat, wp.lon);
        double rel = bearing - ac_hdg;
        while (rel > 180.0) rel -= 360.0;
        while (rel < -180.0) rel += 360.0;
        if (fabs(rel) > (double)ARC_HALF_SWEEP) continue;  // 超出显示范围

        int sx, sy;
        wpt_to_screen(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, sx, sy);

        // 裁剪: 超出弧线范围且不在飞机下方的点隐藏
        int dx = sx - ARC_CX, dy = sy - ARC_CY;
        if (dx * dx + dy * dy > ARC_R * ARC_R && sy > ARC_CY + ARC_R) continue;

        // 蓝色向下三角形
        int d = 7;
        Sint16 vx[3] = {(Sint16)sx, (Sint16)(sx - d), (Sint16)(sx + d)};
        Sint16 vy[3] = {(Sint16)(sy + d), (Sint16)(sy - d), (Sint16)(sy - d)};
        filledPolygonRGBA(r.sdl_rend, vx, vy, 3,
                          Color::BLUE_AC.r, Color::BLUE_AC.g, Color::BLUE_AC.b, 255);
        aapolygonRGBA(r.sdl_rend, vx, vy, 3,
                      255, 255, 255, 200);

        // 白色标签
        r.draw_text(sx + 8, sy - 12, wp.id, Color::WHITE, true);
    }
}

// ---- 导航台符号 (VOR/NDB/Airport/Fix) ----
inline void draw_navaids(NDRenderer& r, GridHashTable& ht,
                         double ac_lat, double ac_lon, double ac_hdg) {
    auto nearby = query_nearby(ht, ac_lat, ac_lon, 80.0);
    for (auto* np : nearby) {
        int sx, sy;
        wpt_to_screen(ac_lat, ac_lon, ac_hdg, np->lat, np->lon, sx, sy);
        int dx = sx - ARC_CX, dy = sy - ARC_CY;
        if (dx * dx + dy * dy > ARC_R * ARC_R && sy > ARC_CY + ARC_R) continue;

        if (np->type == NAV_AIRPORT) {
            circleRGBA(r.sdl_rend, sx, sy, 6, 255, 255, 255, 200);
        } else if (np->type == NAV_VOR) {
            int d = 4;
            Sint16 vx[4] = {(Sint16)sx, (Sint16)(sx + d), (Sint16)sx, (Sint16)(sx - d)};
            Sint16 vy[4] = {(Sint16)(sy - d), (Sint16)sy, (Sint16)(sy + d), (Sint16)sy};
            filledPolygonRGBA(r.sdl_rend, vx, vy, 4,
                              160, 222, 164, 200);
        } else if (np->type == NAV_NDB) {
            filledCircleRGBA(r.sdl_rend, sx, sy, 3, 0, 200, 200, 180);
        } else if (np->type == NAV_FIX) {
            filledTrigonRGBA(r.sdl_rend, sx, sy - 4,
                             sx - 4, sy + 3, sx + 4, sy + 3,
                             120, 120, 120, 160);
        }
    }
}

// ---- 主渲染入口 ----
inline void draw_nd_map(NDRenderer& r, const std::vector<Waypoint>& wpts,
                         GridHashTable& ht) {
    // 1. 罗盘弧线 (最底层)
    draw_arc_compass(r, g_nd.heading);

    // 2. 航路线 (在弧线之上)
    draw_route(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);

    // 3. 导航台
    draw_navaids(r, ht, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);

    // 4. 航路点
    draw_waypoints(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);

    // 5. 中心航迹线
    draw_center_line(r);

    // 6. 飞机符号
    draw_aircraft_symbol(r);

    // 7. 顶部状态栏
    draw_top_mode(r);

    // 8. 左上角速度信息
    draw_speed_wind(r);

    // 9. 右上角航路信息
    draw_next_wpt(r);
}
