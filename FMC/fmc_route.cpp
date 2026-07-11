#include "fmc_route.h"

AVLTree    g_waypoint_tree;
RouteArray g_route;

void load_navdata_avl(const char* nav_path, const char* fix_path, const char* apt_path) {
    int total = 0;
    char line[512];

    // ---- earth_nav.dat (VOR/NDB) ----
    FILE* f = fopen(nav_path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '\n' || line[0] == 'I' || line[0] == ' ') continue;
            int type; double lat, lon, elev; int freq, range; float mag;
            char name[128];
            int n = sscanf(line, "%d %lf %lf %lf %d %d %f %127[^\n]",
                           &type, &lat, &lon, &elev, &freq, &range, &mag, name);
            if (n >= 7) {
                RouteWpt w; w.lat=lat; w.lon=lon; w.freq=freq;
                w.type = (type==3||type==12||type==13)?'V':'N';
                char* p=name; while(*p==' ') p++;
                int il=0; while(p[il]!=' '&&p[il]!='\0') il++;
                if(il>0&&il<15){strncpy(w.id,p,il); w.id[il]='\0';}
                else snprintf(w.id,15,"N%04d",total);
                g_waypoint_tree.insert(w);
                total++;
            }
        }
        fclose(f);
        printf("[AVL] Loaded %d VOR/NDB from nav\n", total);
    }

    // ---- earth_fix.dat (Waypoints) ----
    int fix_count=0;
    f = fopen(fix_path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            double lat, lon; char id[32], rgn[16], cd[16];
            int n = sscanf(line, "%lf %lf %31s %15s %15s", &lat, &lon, id, rgn, cd);
            if (n >= 3 && strlen(id) < 15) {
                RouteWpt w; w.lat=lat; w.lon=lon; w.freq=0; w.type='F';
                strncpy(w.id, id, 15); w.id[15]='\0';
                g_waypoint_tree.insert(w);
                fix_count++;
            }
        }
        fclose(f);
        printf("[AVL] Loaded %d fixes\n", fix_count);
    }

    // ---- apt.dat (Airports) ----
    int apt_count=0;
    f = fopen(apt_path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char icao[16]; double lat, lon;
            int n = sscanf(line, "%15s %lf %lf", icao, &lat, &lon);
            if (n == 3) {
                RouteWpt w; w.lat=lat; w.lon=lon; w.freq=0; w.type='A';
                strncpy(w.id, icao, 15); w.id[15]='\0';
                g_waypoint_tree.insert(w);
                apt_count++;
            }
        }
        fclose(f);
        printf("[AVL] Loaded %d airports\n", apt_count);
    }
    printf("[AVL] Total: %d waypoints in tree\n", g_waypoint_tree.count);
}

void load_route_from_fms(const char* path) {
    g_route.clear();
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[256];
    int added = 0;
    while (fgets(line, sizeof(line), f) && added < MAX_ROUTE_WPTS) {
        int type; char id[64]; double alt, lat, lon;
        int n = sscanf(line, "%d %63s %lf %lf %lf", &type, id, &alt, &lat, &lon);
        if (n >= 5 && type == 11) {
            // 在AVL树中查找详细信息
            AVLNode* found = g_waypoint_tree.search(id);
            RouteWpt w;
            if (found) {
                w = found->wpt;
            } else {
                w.lat=lat; w.lon=lon; w.freq=0; w.type='F';
                strncpy(w.id, id, 15); w.id[15]='\0';
            }
            g_route.append_leg(w.id, w.lat, w.lon);
            added++;
        }
    }
    fclose(f);
    printf("[Route] Loaded %d waypoints from %s\n", added, path);
}
