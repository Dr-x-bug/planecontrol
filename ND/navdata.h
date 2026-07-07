#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Waypoint {
    std::string id;
    double lat;
    double lon;
};

// Load X-Plane .fms flight plan
inline std::vector<Waypoint> load_fms(const std::string& path) {
    std::vector<Waypoint> wpts;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return wpts;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int type;
        char id[64];
        double alt, lat, lon;
        int n = sscanf(line, "%d %63s %lf %lf %lf", &type, id, &alt, &lat, &lon);
        if (n >= 5 && type == 11) {  // 11 = enroute waypoint
            wpts.push_back({id, lat, lon});
        }
    }
    fclose(f);
    return wpts;
}

// Convert lat/lon to screen coordinates relative to aircraft position
inline void latlon_to_xy(double ac_lat, double ac_lon, double ac_hdg,
                         double wp_lat, double wp_lon,
                         int& sx, int& sy) {
    double dlat_nm = (wp_lat - ac_lat) * 60.0;
    double dlon_nm = (wp_lon - ac_lon) * 60.0 * cos(ac_lat * M_PI / 180.0);

    // Rotate by heading
    double hdg_rad = (ac_hdg - 90.0) * M_PI / 180.0;
    double rx =  dlon_nm * cos(hdg_rad) - dlat_nm * sin(hdg_rad);
    double ry = -dlon_nm * sin(hdg_rad) - dlat_nm * cos(hdg_rad);

    sx = ND_CX + (int)(rx / NM_PER_PX);
    sy = ND_CY + (int)(ry / NM_PER_PX);
}
