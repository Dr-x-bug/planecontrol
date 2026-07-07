#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

// ===== 统一导航点类型 =====
enum NavType { NAV_VOR = 0, NAV_NDB = 1, NAV_FIX = 2, NAV_AIRPORT = 3, NAV_TOWER = 4 };

struct NavPoint {
    double   lat, lon;      // 经纬度 (deg)
    NavType  type;           // 导航点类型
    char     id[16];         // 标识符 (ICAO / fix name / VOR id)
    char     name[64];       // 名称
    int      freq;           // 频率 (kHz*100 for VOR/NDB, 0 for others)
    NavPoint* next;          // 哈希链表下一节点
};

// ===== 网格化哈希表 =====
// 网格基准: 1°×1° @ 赤道
// 纬度 φ 处经度网格宽度: BASE_GRID / cos(φ)
// 80nm ≈ 1.33°, 查询范围 = 基准网格 × 2

constexpr double BASE_GRID   = 1.0;     // 基准网格大小 (度)
constexpr double QUERY_RANGE = 80.0;    // 查询范围 (nm)
constexpr int    LAT_BUCKETS = 180;     // 纬度桶数 [-90, 90]
constexpr int    MAX_LON_BUCKETS = 720; // 最大经度桶数 @ 赤道

struct GridHashTable {
    // 二维桶数组: [lat_idx][lon_idx] = 链表头
    NavPoint*** buckets;
    int*       lon_bucket_counts;  // 每个纬度行的桶数
    int        point_count;

    GridHashTable() : buckets(nullptr), lon_bucket_counts(nullptr), point_count(0) {}

    // 获取纬度行索引
    int lat_index(double lat) const {
        int idx = (int)((lat + 90.0) / BASE_GRID);
        if (idx < 0) idx = 0;
        if (idx >= LAT_BUCKETS) idx = LAT_BUCKETS - 1;
        return idx;
    }

    // 获取经度桶数（纬度自适应）
    int lon_buckets_at(double lat) const {
        // cos(lat) 因子调整: 赤道360桶, 极点1桶
        double factor = cos(lat * M_PI / 180.0);
        if (factor < 0.02) factor = 0.02;
        int count = (int)(MAX_LON_BUCKETS * factor);
        if (count < 1) count = 1;
        return count;
    }

    // 获取经度索引
    int lon_index(double lat, double lon) const {
        int max_buckets = lon_buckets_at(lat);
        double grid_w = 360.0 / max_buckets;  // 当前纬度的网格宽度
        int idx = (int)((lon + 180.0) / grid_w);
        if (idx < 0) idx = 0;
        if (idx >= max_buckets) idx = max_buckets - 1;
        return idx;
    }

    // 初始化哈希表
    void init() {
        buckets = (NavPoint***)calloc(LAT_BUCKETS, sizeof(NavPoint**));
        lon_bucket_counts = (int*)calloc(LAT_BUCKETS, sizeof(int));

        // 预计算每行的桶数并分配
        for (int i = 0; i < LAT_BUCKETS; i++) {
            double lat = -90.0 + i * BASE_GRID + BASE_GRID / 2.0;
            int n = lon_buckets_at(lat);
            lon_bucket_counts[i] = n;
            buckets[i] = (NavPoint**)calloc(n, sizeof(NavPoint*));
        }
        point_count = 0;
    }

    // 插入导航点 (链表法解决哈希冲突)
    void insert(double lat, double lon, NavType type,
                const char* id, const char* name, int freq) {
        int li = lat_index(lat);
        int lj = lon_index(lat, lon);

        NavPoint* np = (NavPoint*)malloc(sizeof(NavPoint));
        np->lat  = lat;
        np->lon  = lon;
        np->type = type;
        np->freq = freq;
        strncpy(np->id,   id   ? id   : "", 15);
        strncpy(np->name, name ? name : "", 63);
        np->id[15]   = '\0';
        np->name[63] = '\0';

        // 头插法到链表
        np->next = buckets[li][lj];
        buckets[li][lj] = np;
        point_count++;
    }

    // 释放内存
    void destroy() {
        for (int i = 0; i < LAT_BUCKETS; i++) {
            if (!buckets[i]) continue;
            int n = lon_bucket_counts[i];
            for (int j = 0; j < n; j++) {
                NavPoint* p = buckets[i][j];
                while (p) {
                    NavPoint* next = p->next;
                    free(p);
                    p = next;
                }
            }
            free(buckets[i]);
        }
        free(buckets);
        free(lon_bucket_counts);
    }
};

// ===== 数据加载函数 =====

// 加载 earth_nav.dat (VOR/NDB/DME, 37119行)
inline int load_navaids(GridHashTable& ht, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    char line[512];
    int count = 0;
    int skipped = 0;
    while (fgets(line, sizeof(line), f)) {
        // 跳过空行和文件头
        if (line[0] == '\n' || line[0] == '\r' || line[0] == 'I') continue;
        if (line[0] == ' ' && line[1] == ' ') continue;  // 空行

        int type;
        double lat, lon, elev;
        int freq, range_val;
        float mag_var;
        char name[128];
        char icao[16];

        // 尝试解析 (支持类型 2,3,12,13等)
        int n = sscanf(line, "%d %lf %lf %lf %d %d %f %127[^\n]",
                       &type, &lat, &lon, &elev, &freq, &range_val, &mag_var, name);
        if (n >= 7) {
            // 提取ICAO代码 (名称前4个字母)
            char* p = name;
            while (*p == ' ') p++;
            // name格式: "ABC ENRT DN ABUJA VOR/DME"
            // ICAO = 前3-4个字母
            int id_len = 0;
            while (p[id_len] != ' ' && p[id_len] != '\0') id_len++;
            if (id_len > 0 && id_len < 8) {
                strncpy(icao, p, id_len);
                icao[id_len] = '\0';
            } else {
                snprintf(icao, sizeof(icao), "NAV%04d", count);
            }

            NavType nav_type = (type == 3 || type == 12 || type == 13) ? NAV_VOR : NAV_NDB;
            ht.insert(lat, lon, nav_type, icao, name, freq);
            count++;
        } else {
            skipped++;
        }
    }
    fclose(f);
    printf("[Hash] Loaded %d navaids (skipped %d) from %s\n", count, skipped, path);
    return count;
}

// 加载 earth_fix.dat (航路点, 198608行)
inline int load_fixes(GridHashTable& ht, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        double lat, lon;
        char id[32], region[16], code[16];
        // 格式: lat lon id type region code
        int n = sscanf(line, "%lf %lf %31s %15s %15s", &lat, &lon, id, region, code);
        if (n >= 3) {
            ht.insert(lat, lon, NAV_FIX, id, "", 0);
            count++;
        }
    }
    fclose(f);
    printf("[Hash] Loaded %d fixes from %s\n", count, path);
    return count;
}

// 加载 apt.dat (机场, 12678行)
inline int load_airports(GridHashTable& ht, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        char icao[16];
        double lat, lon;
        // 格式: ICAO lat lon
        int n = sscanf(line, "%15s %lf %lf", icao, &lat, &lon);
        if (n == 3) {
            ht.insert(lat, lon, NAV_AIRPORT, icao, icao, 0);
            count++;
        }
    }
    fclose(f);
    printf("[Hash] Loaded %d airports from %s\n", count, path);
    return count;
}

// ===== 附近范围查询 =====
// 查找指定位置80nm内的所有导航点
// 核心算法: 计算包含查询范围的网格单元 → 遍历单元内链表 → 距离过滤

inline std::vector<NavPoint*> query_nearby(GridHashTable& ht,
                                            double center_lat, double center_lon,
                                            double range_nm = QUERY_RANGE) {
    std::vector<NavPoint*> results;

    // 80nm ≈ 1.333° 纬度
    double lat_range = range_nm / 60.0;
    double lon_range = range_nm / (60.0 * cos(center_lat * M_PI / 180.0) + 0.001);

    // 确定需要检查的网格范围
    int lat_start = ht.lat_index(center_lat - lat_range);
    int lat_end   = ht.lat_index(center_lat + lat_range);
    if (lat_start < 0) lat_start = 0;
    if (lat_end >= LAT_BUCKETS) lat_end = LAT_BUCKETS - 1;

    // 遍历范围内的网格
    for (int li = lat_start; li <= lat_end; li++) {
        double grid_center_lat = -90.0 + li * BASE_GRID + BASE_GRID / 2.0;
        int lon_start = ht.lon_index(grid_center_lat, center_lon - lon_range);
        int lon_end   = ht.lon_index(grid_center_lat, center_lon + lon_range);
        int max_lon   = ht.lon_bucket_counts[li];
        if (lon_start < 0) lon_start = 0;
        if (lon_end >= max_lon) lon_end = max_lon - 1;

        for (int lj = lon_start; lj <= lon_end; lj++) {
            NavPoint* p = ht.buckets[li][lj];
            while (p) {
                // 精确距离计算 (Haversine)
                double dlat = (p->lat - center_lat) * M_PI / 180.0;
                double dlon = (p->lon - center_lon) * M_PI / 180.0;
                double a = sin(dlat/2)*sin(dlat/2) +
                          cos(center_lat*M_PI/180.0)*cos(p->lat*M_PI/180.0)*
                          sin(dlon/2)*sin(dlon/2);
                double c = 2 * atan2(sqrt(a), sqrt(1-a));
                double dist_nm = 3440.065 * c;  // 地球半径 (nm)

                if (dist_nm <= range_nm) {
                    results.push_back(p);
                }
                p = p->next;
            }
        }
    }

    return results;
}
