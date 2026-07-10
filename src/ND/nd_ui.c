/**
 * nd_ui.c — ND 模块界面层实现
 * 职责: NDRenderer 初始化/销毁、所有 ND 绘制函数、工具函数
 */
#include "nd_ui.h"
#include "nd_data.h"
#include <cstdio>
#include <cmath>
#include <vector>

// ============================================================
//  NDRenderer 方法实现
// ============================================================

bool NDRenderer::init() {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    window = SDL_CreateWindow("ND - MAP Mode",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        ND_WIN_W, ND_WIN_H, SDL_WINDOW_SHOWN);
    sdl_rend = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    font    = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 18);
    font_sm = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 14);
    return window && sdl_rend && font;
}

void NDRenderer::clear() {
    SDL_SetRenderDrawColor(sdl_rend, 0, 0, 0, 255);
    SDL_RenderClear(sdl_rend);
}

void NDRenderer::present() { SDL_RenderPresent(sdl_rend); }

void NDRenderer::set_color(SDL_Color c) {
    SDL_SetRenderDrawColor(sdl_rend, c.r, c.g, c.b, c.a);
}

void NDRenderer::draw_text(int x, int y, const std::string& txt, SDL_Color c, bool small) {
    if (txt.empty()) return;
    TTF_Font* f = small ? font_sm : font;
    SDL_Surface* surf = TTF_RenderText_Blended(f, txt.c_str(), c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_rend, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(sdl_rend, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void NDRenderer::draw_text_center(int cx, int y, const std::string& txt, SDL_Color c, bool small) {
    if (txt.empty()) return;
    TTF_Font* f = small ? font_sm : font;
    SDL_Surface* surf = TTF_RenderText_Blended(f, txt.c_str(), c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_rend, surf);
    SDL_Rect dst = {cx - surf->w/2, y, surf->w, surf->h};
    SDL_RenderCopy(sdl_rend, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void NDRenderer::draw_text_right(int rx, int y, const std::string& txt, SDL_Color c, bool small) {
    if (txt.empty()) return;
    TTF_Font* f = small ? font_sm : font;
    SDL_Surface* surf = TTF_RenderText_Blended(f, txt.c_str(), c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_rend, surf);
    SDL_Rect dst = {rx - surf->w, y, surf->w, surf->h};
    SDL_RenderCopy(sdl_rend, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

NDRenderer::~NDRenderer() {
    if (font_sm)  TTF_CloseFont(font_sm);
    if (font)     TTF_CloseFont(font);
    if (sdl_rend) SDL_DestroyRenderer(sdl_rend);
    if (window)   SDL_DestroyWindow(window);
}

// ============================================================
//  工具函数
// ============================================================

double normalize_angle(double angle) {
    double v = fmod(angle, 360.0);
    if (v < 0) v += 360.0;
    return v;
}

double calc_bearing_deg(double lat1, double lon1, double lat2, double lon2) {
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double rlat1 = lat1 * M_PI / 180.0, rlat2 = lat2 * M_PI / 180.0;
    double y = sin(dLon) * cos(rlat2);
    double x = cos(rlat1) * sin(rlat2) - sin(rlat1) * cos(rlat2) * cos(dLon);
    double brng = atan2(y, x) * 180.0 / M_PI;
    if (brng < 0) brng += 360.0;
    return brng;
}

double calc_dist_nm(double lat1, double lon1, double lat2, double lon2) {
    double R = 3440.065;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat/2)*sin(dlat/2) +
               cos(lat1*M_PI/180.0)*cos(lat2*M_PI/180.0)*sin(dlon/2)*sin(dlon/2);
    return R * 2 * atan2(sqrt(a), sqrt(1-a));
}

void wpt_to_screen(double ac_lat, double ac_lon, double ac_hdg,
                   double wpt_lat, double wpt_lon, int& sx, int& sy) {
    double dist_nm = calc_dist_nm(ac_lat, ac_lon, wpt_lat, wpt_lon);
    double bearing = calc_bearing_deg(ac_lat, ac_lon, wpt_lat, wpt_lon);
    double px = dist_nm / NM_PER_PX;
    double rad = (bearing - ac_hdg) * M_PI / 180.0;
    sx = ARC_CX + (int)(px * sin(rad));
    sy = ARC_CY - (int)(px * cos(rad));
}

int arc_heading_label(double hdg, int arc_angle) {
    double bearing = normalize_angle(hdg + (270.0 - (double)arc_angle));
    int label = (int)round(bearing / 10.0);
    if (label >= 36) label -= 36;
    return label;
}

// ============================================================
//  绘制函数
// ============================================================

void draw_top_mode(NDRenderer& r) {
    int hdg = (int)round(normalize_angle(g_nd.heading));
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", hdg);
    int box_w = 28, box_h = 22, box_y = 4;
    int box_x = ARC_CX - box_w/2;
    rectangleRGBA(r.sdl_rend, box_x, box_y, box_x+box_w, box_y+box_h, 255, 255, 255, 255);
    r.draw_text_center(ARC_CX, box_y+3, buf, Color::WHITE, false);
    r.draw_text(box_x - 55, box_y+3, "TRK", Color::GREEN_RTE, true);
    r.draw_text(box_x + box_w + 10, box_y+3, "MAG", Color::GREEN_RTE, true);
    int tri_y = box_y + box_h + 1;
    Sint16 vx[3] = {(Sint16)ARC_CX, (Sint16)(ARC_CX - 7), (Sint16)(ARC_CX + 7)};
    Sint16 vy[3] = {(Sint16)(tri_y + 10), (Sint16)tri_y, (Sint16)tri_y};
    filledPolygonRGBA(r.sdl_rend, vx, vy, 3, 255, 255, 255, 255);
}

void draw_speed_wind(NDRenderer& r) {
    char buf[64];
    snprintf(buf, sizeof(buf), "GS: %.0f TAS: %.0f", g_nd.gs, g_nd.tas);
    r.draw_text(12, 10, buf, Color::WHITE, false);
    r.draw_text(12, 34, "---\u00b0/---", Color::CYAN, true);
}

void draw_next_wpt(NDRenderer& r) {
    int rx = ND_WIN_W - 12;
    r.draw_text_right(rx, 10, "RW02L", Color::MAGENTA_WP, false);
    r.draw_text_right(rx, 36, "MIN", Color::WHITE, true);
    r.draw_text_right(rx, 58, "01.0NM", Color::CYAN, true);
}

void draw_arc_compass(NDRenderer& r, double hdg) {
    int cx = ARC_CX, cy = ARC_CY, rad = ARC_R;

    arcRGBA(r.sdl_rend, cx, cy, rad, ARC_START, ARC_END, 255, 255, 255, 220);
    arcRGBA(r.sdl_rend, cx, cy, rad+1, ARC_START, ARC_END, 255, 255, 255, 180);
    arcRGBA(r.sdl_rend, cx, cy, rad+2, ARC_START, ARC_END, 255, 255, 255, 100);

    for (int a = ARC_START; a <= ARC_END; a += 5) {
        double ra = a * M_PI / 180.0;
        double cosa = cos(ra), sina = sin(ra);
        bool major = (a % 10 == 0);
        int tickLen = major ? 22 : 10;
        int alpha   = major ? 220 : 130;

        int x1 = cx + (int)((rad - 1) * cosa);
        int y1 = cy + (int)((rad - 1) * sina);
        int x2 = cx + (int)((rad - 1 - tickLen) * cosa);
        int y2 = cy + (int)((rad - 1 - tickLen) * sina);
        lineRGBA(r.sdl_rend, x1, y1, x2, y2, 255, 255, 255, alpha);

        if (major) {
            int lbl = arc_heading_label(hdg, a);
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", lbl);
            int tx = cx + (int)((rad - 28) * cosa) - (lbl >= 10 ? 10 : 5);
            int ty = cy + (int)((rad - 32) * sina) - 9;
            r.draw_text(tx, ty, buf, Color::WHITE, true);
        }
    }

    int px = cx + (int)(rad * cos(270.0 * M_PI / 180.0));
    int py = cy + (int)(rad * sin(270.0 * M_PI / 180.0));
    Sint16 tri_x[3] = {(Sint16)px, (Sint16)(px - 9), (Sint16)(px + 9)};
    Sint16 tri_y[3] = {(Sint16)(py - 14), (Sint16)py, (Sint16)py};
    filledPolygonRGBA(r.sdl_rend, tri_x, tri_y, 3, 255, 255, 255, 255);
}

void draw_center_line(NDRenderer& r) {
    int top = ARC_CY - ARC_R;
    lineRGBA(r.sdl_rend, ARC_CX, top, ARC_CX, ACFT_Y - 16, 255, 255, 255, 100);
}

void draw_aircraft_symbol(NDRenderer& r) {
    int cx = ARC_CX, cy = ACFT_Y, sz = 14;
    Sint16 vx[3] = {(Sint16)cx, (Sint16)(cx - sz), (Sint16)(cx + sz)};
    Sint16 vy[3] = {(Sint16)(cy - sz), (Sint16)(cy + sz/2), (Sint16)(cy + sz/2)};
    aapolygonRGBA(r.sdl_rend, vx, vy, 3, 255, 255, 255, 255);
    filledCircleRGBA(r.sdl_rend, cx, cy, 4, 255, 255, 255, 255);
    int rdot = 20;
    for (int i = 0; i < 8; i++) {
        double ang = i * M_PI / 4.0;
        int dx = (int)(rdot * cos(ang));
        int dy = (int)(rdot * sin(ang));
        filledCircleRGBA(r.sdl_rend, cx + dx, cy - dy, 2, 200, 200, 200, 200);
    }
    lineRGBA(r.sdl_rend, cx, cy - sz, cx, ARC_CY - ARC_R, 255, 255, 255, 120);
}

void draw_route(NDRenderer& r, const std::vector<Waypoint>& wpts,
                double ac_lat, double ac_lon, double ac_hdg) {
    if (wpts.size() < 2) return;
    std::vector<int> sx, sy;
    sx.reserve(wpts.size());
    sy.reserve(wpts.size());
    for (const auto& wp : wpts) {
        int px, py;
        wpt_to_screen(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, px, py);
        sx.push_back(px); sy.push_back(py);
    }
    for (size_t i = 0; i < sx.size() - 1; i++) {
        thickLineRGBA(r.sdl_rend, sx[i]+2, sy[i]+2, sx[i+1]+2, sy[i+1]+2,
                      4, Color::PURPLE.r, Color::PURPLE.g, Color::PURPLE.b, 90);
    }
    for (size_t i = 0; i < sx.size() - 1; i++) {
        thickLineRGBA(r.sdl_rend, sx[i], sy[i], sx[i+1], sy[i+1],
                      3, Color::GREEN_RTE.r, Color::GREEN_RTE.g, Color::GREEN_RTE.b, 240);
        thickLineRGBA(r.sdl_rend, sx[i], sy[i], sx[i+1], sy[i+1],
                      1, Color::GREEN_LT.r, Color::GREEN_LT.g, Color::GREEN_LT.b, 255);
    }
}

void draw_waypoints(NDRenderer& r, const std::vector<Waypoint>& wpts,
                    double ac_lat, double ac_lon, double ac_hdg) {
    for (size_t idx = 0; idx < wpts.size(); idx++) {
        const auto& wp = wpts[idx];
        double dist_nm = calc_dist_nm(ac_lat, ac_lon, wp.lat, wp.lon);
        if (dist_nm > 80.0) continue;
        double bearing = calc_bearing_deg(ac_lat, ac_lon, wp.lat, wp.lon);
        double rel = bearing - ac_hdg;
        while (rel > 180.0) rel -= 360.0;
        while (rel < -180.0) rel += 360.0;
        if (fabs(rel) > 45.0) continue;

        int sx, sy;
        wpt_to_screen(ac_lat, ac_lon, ac_hdg, wp.lat, wp.lon, sx, sy);
        int dx = sx - ARC_CX, dy = sy - ARC_CY;
        if (dx*dx + dy*dy > ARC_R * ARC_R && sy > ARC_CY + ARC_R) continue;

        int d = 7;
        Sint16 vx[3] = {(Sint16)sx, (Sint16)(sx - d), (Sint16)(sx + d)};
        Sint16 vy[3] = {(Sint16)(sy + d), (Sint16)(sy - d), (Sint16)(sy - d)};
        filledPolygonRGBA(r.sdl_rend, vx, vy, 3,
                          Color::BLUE_AC.r, Color::BLUE_AC.g, Color::BLUE_AC.b, 255);
        aapolygonRGBA(r.sdl_rend, vx, vy, 3, 255, 255, 255, 200);
        r.draw_text(sx + 8, sy - 12, wp.id, Color::WHITE, true);
    }
}

void draw_bottom_info(NDRenderer& r) {
    char buf[64];
    snprintf(buf, sizeof(buf), "HDG %.0f", g_nd.heading);
    r.draw_text(12, ND_WIN_H - 28, buf, Color::WHITE, true);
    r.draw_text_right(ND_WIN_W - 12, ND_WIN_H - 28, "RNG 20 NM", Color::CYAN, true);
}

// ============================================================
//  主渲染入口
// ============================================================

void draw_nd_map(NDRenderer& r, const std::vector<Waypoint>& wpts,
                 GridHashTable& ht) {
    (void)ht;
    draw_arc_compass(r, g_nd.heading);
    draw_top_mode(r);
    draw_route(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);
    draw_waypoints(r, wpts, g_nd.ac_lat, g_nd.ac_lon, g_nd.heading);
    draw_center_line(r);
    draw_aircraft_symbol(r);
    draw_speed_wind(r);
    draw_next_wpt(r);
}
