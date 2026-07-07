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

    // Outer ring
    circleRGBA(r.sdl_rend, cx, cy, rad, Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, 80);
    circleRGBA(r.sdl_rend, cx, cy, rad - 1, Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, 50);

    // Range ring (half scale)
    circleRGBA(r.sdl_rend, cx, cy, rad / 2, Color::GRAY.r, Color::GRAY.g, Color::GRAY.b, 60);

    // Heading ticks every 5 degrees
    for (int deg = 0; deg < 360; deg += 5) {
        double a = (deg - hdg + 90.0) * M_PI / 180.0;
        int len = (deg % 10 == 0) ? 12 : 6;
        int x1 = cx + (int)((rad - 2) * cos(a));
        int y1 = cy - (int)((rad - 2) * sin(a));
        int x2 = cx + (int)((rad - 2 - len) * cos(a));
        int y2 = cy - (int)((rad - 2 - len) * sin(a));
        lineRGBA(r.sdl_rend, x1, y1, x2, y2,
            Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, 160);

        // Cardinal labels every 30 degrees
        if (deg % 30 == 0) {
            int lbl = deg / 10;
            if (lbl == 0) lbl = 36;
            char buf[8];
            snprintf(buf, sizeof(buf), "%02d", lbl);
            int tx = cx + (int)((rad - 22) * cos(a)) - 10;
            int ty = cy - (int)((rad - 22) * sin(a)) - 8;
            r.draw_text(tx, ty, buf, Color::WHITE, true);
        }
    }

    // North indicator (triangle)
    double na = (0.0 - hdg + 90.0) * M_PI / 180.0;
    int nx = cx + (int)((rad + 8) * cos(na));
    int ny = cy - (int)((rad + 8) * sin(na));
    filledTrigonRGBA(r.sdl_rend, nx, ny - 6, nx - 6, ny + 6, nx + 6, ny + 6,
        Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, 255);
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

    // Route shadow (purple offset)
    for (size_t i = 0; i < sx_list.size() - 1; i++) {
        lineRGBA(r.sdl_rend, sx_list[i] + 1, sy_list[i] + 1,
            sx_list[i+1] + 1, sy_list[i+1] + 1,
            Color::PURPLE.r, Color::PURPLE.g, Color::PURPLE.b, 100);
    }

    // Route line (green)
    for (size_t i = 0; i < sx_list.size() - 1; i++) {
        thickLineRGBA(r.sdl_rend, sx_list[i], sy_list[i],
            sx_list[i+1], sy_list[i+1], 3,
            Color::GREEN_LT.r, Color::GREEN_LT.g, Color::GREEN_LT.b, 200);
        thickLineRGBA(r.sdl_rend, sx_list[i], sy_list[i],
            sx_list[i+1], sy_list[i+1], 1,
            Color::GREEN_RTE.r, Color::GREEN_RTE.g, Color::GREEN_RTE.b, 255);
    }
}

// ===== 3. Waypoint Markers (航路点标识) =====
inline void draw_waypoints(NDRenderer& r, const std::vector<Waypoint>& wpts,
                           double ac_lat, double ac_lon, double ac_hdg) {
    for (const auto& wp : wpts) {
        int sx, sy;
        latlon_to_xy(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, sx, sy);

        // Only draw if within display
        int dx = sx - ND_CX, dy = sy - ND_CY;
        if (dx*dx + dy*dy > ND_RADIUS * ND_RADIUS) continue;

        // Diamond marker (magenta)
        int d = 5;
        Sint16 vx[4] = {(Sint16)(sx), (Sint16)(sx + d), (Sint16)(sx), (Sint16)(sx - d)};
        Sint16 vy[4] = {(Sint16)(sy - d), (Sint16)(sy), (Sint16)(sy + d), (Sint16)(sy)};
        filledPolygonRGBA(r.sdl_rend, vx, vy, 4,
            Color::MAGENTA_WP.r, Color::MAGENTA_WP.g, Color::MAGENTA_WP.b, 255);
        aapolygonRGBA(r.sdl_rend, vx, vy, 4,
            Color::MAGENTA_WP.r, Color::MAGENTA_WP.g, Color::MAGENTA_WP.b, 255);

        // Waypoint label
        r.draw_text(sx + 8, sy - 6, wp.id, Color::MAGENTA_WP, true);
    }
}

// ===== 4. Aircraft Symbol (本机位置) =====
inline void draw_aircraft(NDRenderer& r) {
    int cx = ND_CX, cy = ND_CY;
    int sz = 14;

    // White aircraft triangle
    filledTrigonRGBA(r.sdl_rend,
        cx, cy - sz,           // nose
        cx - sz/2, cy + sz/2,  // left wing
        cx + sz/2, cy + sz/2,  // right wing
        Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, 255);

    // Center dot (blue)
    filledCircleRGBA(r.sdl_rend, cx, cy, 3,
        Color::BLUE_AC.r, Color::BLUE_AC.g, Color::BLUE_AC.b, 255);
}

// ===== 5. Speed Display (速度及风速显示) =====
inline void draw_speed_info(NDRenderer& r, double gs, double tas, double wind_dir, double wind_spd) {
    // Top-left corner info box
    char buf[64];

    // GS - Ground Speed
    snprintf(buf, sizeof(buf), "GS  %.0f kt", gs);
    r.draw_text(10, 8, buf, Color::WHITE, true);

    // TAS - True Airspeed
    snprintf(buf, sizeof(buf), "TAS %.0f kt", tas);
    r.draw_text(10, 26, buf, Color::GREEN_LT, true);

    // Wind direction arrow + speed (top-right)
    int wx = ND_WIN_W - 130, wy = 25;
    double w_rad = (wind_dir - 180.0) * M_PI / 180.0;  // direction TO
    int arr_len = 30;
    int ax1 = wx - (int)(arr_len/2 * cos(w_rad));
    int ay1 = wy + (int)(arr_len/2 * sin(w_rad));
    int ax2 = wx + (int)(arr_len/2 * cos(w_rad));
    int ay2 = wy - (int)(arr_len/2 * sin(w_rad));

    // Wind arrow shaft
    lineRGBA(r.sdl_rend, ax1, ay1, ax2, ay2,
        Color::CYAN.r, Color::CYAN.g, Color::CYAN.b, 255);

    // Arrow head
    int head_len = 8;
    int hx1 = ax2 - (int)(head_len * cos(w_rad - 0.5));
    int hy1 = ay2 + (int)(head_len * sin(w_rad - 0.5));
    int hx2 = ax2 - (int)(head_len * cos(w_rad + 0.5));
    int hy2 = ay2 + (int)(head_len * sin(w_rad + 0.5));
    lineRGBA(r.sdl_rend, ax2, ay2, hx1, hy1, Color::CYAN.r, Color::CYAN.g, Color::CYAN.b, 255);
    lineRGBA(r.sdl_rend, ax2, ay2, hx2, hy2, Color::CYAN.r, Color::CYAN.g, Color::CYAN.b, 255);

    // Wind speed text
    snprintf(buf, sizeof(buf), "%.0f°/%.0fkt", wind_dir, wind_spd);
    r.draw_text(ND_WIN_W - 180, 8, buf, Color::CYAN, true);

    // HDG display (bottom-left)
    snprintf(buf, sizeof(buf), "HDG %.0f", g_nd.heading);
    r.draw_text(10, ND_WIN_H - 25, buf, Color::WHITE, true);

    // Range display (bottom-right)
    snprintf(buf, sizeof(buf), "RNG 20 NM");
    r.draw_text(ND_WIN_W - 80, ND_WIN_H - 25, buf, Color::CYAN, true);
}

// ===== Main ND MAP render =====
inline void draw_nd_map(NDRenderer& r, const std::vector<Waypoint>& wpts,
                         GridHashTable& ht) {
    // Layer 1: Compass rose (background layer)
    draw_compass_rose(r, g_nd.heading);

    // Layer 2: Route line (between compass and waypoints)
    draw_route(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);

    // Layer 3: Waypoint markers (on top of route)
    draw_waypoints(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);

    // Layer 3.5: Nearby navaids from hash table
    auto nearby = query_nearby(ht, g_nd.ac_lat, g_nd.ac_lon, 80.0);
    for (auto* np : nearby) {
        int sx, sy;
        latlon_to_xy(g_nd.ac_lat, g_nd.ac_lon, g_nd.heading,
                     np->lat, np->lon, sx, sy);
        int dx = sx - ND_CX, dy = sy - ND_CY;
        if (dx*dx + dy*dy > ND_RADIUS * ND_RADIUS) continue;
        SDL_Color c = Color::GRAY; int sz = 3;
        if (np->type == NAV_AIRPORT) { c = Color::WHITE; sz = 5; }
        else if (np->type == NAV_VOR) { c = Color::GREEN_LT; sz = 4; }
        else if (np->type == NAV_NDB) { c = Color::CYAN; sz = 3; }
        else continue;
        filledCircleRGBA(r.sdl_rend, sx, sy, sz, c.r, c.g, c.b, 180);
        if (np->type == NAV_AIRPORT)
            r.draw_text(sx + 6, sy - 5, np->id, c, true);
    }

    // Layer 4: Aircraft symbol (topmost on map)
    draw_aircraft(r);

    // Layer 5: Speed & wind info overlay (always on top)
    draw_speed_info(r, g_nd.gs, g_nd.tas, g_nd.wind_dir, g_nd.wind_spd);
}
