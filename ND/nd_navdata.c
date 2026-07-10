/**
 * nd_navdata.c — ND 导航数据层实现
 * 网格化哈希表 + DJB2哈希 + 航向优先网格排序搜索
 */
#include "nd_navdata.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cerrno>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  全局变量定义
// ============================================================

int                waypoint_total_count = 0;
WaypointHashTable* wp_hash_table        = nullptr;
int                wp_result_total      = 0;
WAYPOINT_RESULT*   wp_result            = nullptr;

// ============================================================
//  工具函数
// ============================================================

// Haversine 公式计算两点距离 (km)
double calculate_distance_km(double lat1, double lon1, double lat2, double lon2) {
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dlon / 2.0) * sin(dlon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return EARTH_RADIUS_KM * c;
}

// 计算网格键 (如 33.5°lat, 9.2°lon → "33_9")
static void calc_grid_key(double lat, double lon, char* grid_key, int key_len) {
    int lat_grid = (int)(lat / GRID_SIZE);
    int lon_grid = (int)(lon / GRID_SIZE);
    snprintf(grid_key, key_len, "%d_%d", lat_grid, lon_grid);
}

// DJB2 哈希算法
static unsigned int wp_ht_hash(const char* grid_key, int bucket_size) {
    unsigned long hash = 5381;
    int c;
    while ((c = *grid_key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % bucket_size;
}

// ============================================================
//  哈希表操作
// ============================================================

// 初始化哈希表
static WaypointHashTable* wp_ht_init(int bucket_size) {
    WaypointHashTable* ht = (WaypointHashTable*)malloc(sizeof(WaypointHashTable));
    if (!ht) return nullptr;

    ht->bucket_size = bucket_size;
    ht->buckets = (HashNode**)calloc(bucket_size, sizeof(HashNode*));
    if (!ht->buckets) {
        free(ht);
        return nullptr;
    }
    return ht;
}

// 插入航点到哈希表
static int wp_ht_insert(WaypointHashTable* ht, const NAV_WAYPOINT* wp) {
    if (!ht || !wp) return -1;

    char grid_key[20] = {0};
    calc_grid_key(wp->lat, wp->lon, grid_key, sizeof(grid_key));
    unsigned int bucket_idx = wp_ht_hash(grid_key, ht->bucket_size);

    // 查找网格是否已存在
    HashNode* curr = ht->buckets[bucket_idx];
    HashNode* target_node = nullptr;
    while (curr) {
        if (strcmp(curr->grid_key, grid_key) == 0) {
            target_node = curr;
            break;
        }
        curr = curr->next;
    }

    // 不存在则创建新节点
    if (!target_node) {
        target_node = (HashNode*)malloc(sizeof(HashNode));
        if (!target_node) return -1;

        strncpy(target_node->grid_key, grid_key, sizeof(target_node->grid_key) - 1);
        target_node->grid_key[sizeof(target_node->grid_key) - 1] = '\0';
        target_node->wp_capacity = 100;
        target_node->wp_list = (NAV_WAYPOINT*)malloc(sizeof(NAV_WAYPOINT) * target_node->wp_capacity);
        target_node->wp_count = 0;
        target_node->next = ht->buckets[bucket_idx];  // 头插法
        ht->buckets[bucket_idx] = target_node;
    }

    // 动态扩容
    if (target_node->wp_count >= target_node->wp_capacity) {
        target_node->wp_capacity *= 2;
        NAV_WAYPOINT* new_list = (NAV_WAYPOINT*)realloc(target_node->wp_list,
            sizeof(NAV_WAYPOINT) * target_node->wp_capacity);
        if (!new_list) return -1;
        target_node->wp_list = new_list;
    }

    // 插入
    memcpy(&target_node->wp_list[target_node->wp_count++], wp, sizeof(NAV_WAYPOINT));
    return 0;
}

// ============================================================
//  X-Plane 数据获取
// ============================================================

int getNDNavData(XPCSocket sock, NDNavData* data) {
    const char* drefs[] = {
        "sim/flightmodel/position/latitude",
        "sim/flightmodel/position/longitude",
        "sim/flightmodel/position/mag_psi",
        "sim/flightmodel/position/true_airspeed",
        "sim/flightmodel/position/groundspeed",
    };
    float values[5];
    float* value_ptrs[] = { &values[0], &values[1], &values[2], &values[3], &values[4] };
    int sizes[5] = {1, 1, 1, 1, 1};

    int result = getDREFs(sock, drefs, value_ptrs, 5, sizes);
    if (result < 0) {
        printf("[NDNav] X-Plane data fetch failed\n");
        return -1;
    }

    data->latitude       = values[0];
    data->longitude      = values[1];
    data->heading        = values[2];
    data->true_air_speed = values[3] * 1.94384f;  // m/s → kt
    data->ground_speed   = values[4] * 1.94384f;

    printf("[NDNav] Lat=%.4f Lon=%.4f HDG=%.0f GS=%.0fkt TAS=%.0fkt\n",
           data->latitude, data->longitude, data->heading,
           data->ground_speed, data->true_air_speed);
    return 0;
}

// ============================================================
//  数据加载
// ============================================================

int load_all_nav_data() {
    int total_loaded = 0;

    // 1. 初始化哈希表
    if (!wp_hash_table) {
        wp_hash_table = wp_ht_init(HASH_BUCKET_SIZE);
        if (!wp_hash_table) {
            fprintf(stderr, "[NDNav] Hash table init failed!\n");
            return -1;
        }
    }

    // ========== 加载 earth_fix.dat ==========
    char fix_path[256];
    snprintf(fix_path, sizeof(fix_path), "%s%s", DATA_ROOT_PATH, "earth_fix.dat");
    FILE* fp_fix = fopen(fix_path, "r");
    int fix_loaded = 0;

    if (fp_fix) {
        char line[1024];
        // 跳过前 3 行文件头
        for (int i = 0; i < 3; i++) {
            if (!fgets(line, sizeof(line), fp_fix)) break;
        }

        while (fgets(line, sizeof(line), fp_fix) && total_loaded < MAX_TOTAL_WAYPOINTS) {
            if (line[0] == '\n' || line[0] == '#') continue;

            double first_value;
            if (sscanf(line, "%lf", &first_value) == 1 && first_value == 99.0) {
                printf("[NDNav] earth_fix.dat: hit 99 terminator\n");
                break;
            }

            NAV_WAYPOINT wp;
            memset(&wp, 0, sizeof(wp));
            wp.num = 1;
            if (sscanf(line, "%lf %lf %19s", &wp.lat, &wp.lon, wp.name) >= 2) {
                wp.name[sizeof(wp.name) - 1] = '\0';
                if (wp_ht_insert(wp_hash_table, &wp) == 0) {
                    fix_loaded++; total_loaded++;
                }
            }
        }
        fclose(fp_fix);
        printf("[NDNav] Loaded %d fixes from earth_fix.dat\n", fix_loaded);
    } else {
        fprintf(stderr, "[NDNav] Cannot open %s: %s\n", fix_path, strerror(errno));
    }

    // ========== 加载 earth_nav.dat ==========
    char nav_path[256];
    snprintf(nav_path, sizeof(nav_path), "%s%s", DATA_ROOT_PATH, "earth_nav.dat");
    FILE* fp_nav = fopen(nav_path, "r");
    int nav_loaded = 0;

    if (fp_nav) {
        char line[1024];
        for (int i = 0; i < 3; i++) {
            if (!fgets(line, sizeof(line), fp_nav)) break;
        }

        while (fgets(line, sizeof(line), fp_nav) && total_loaded < MAX_TOTAL_WAYPOINTS) {
            if (line[0] == '\n' || line[0] == '#') continue;

            char first_str[10];
            if (sscanf(line, "%9s", first_str) == 1 && strcmp(first_str, "99") == 0) {
                printf("[NDNav] earth_nav.dat: hit 99 terminator\n");
                break;
            }

            NAV_WAYPOINT wp;
            memset(&wp, 0, sizeof(wp));
            int record_type;
            char name[20];
            if (sscanf(line, "%d %lf %lf %*f %*s %*d %*f %*s %19s",
                       &record_type, &wp.lat, &wp.lon, name) >= 4) {
                wp.num = record_type;
                strncpy(wp.name, name, sizeof(wp.name) - 1);
                wp.name[sizeof(wp.name) - 1] = '\0';
                if (wp_ht_insert(wp_hash_table, &wp) == 0) {
                    nav_loaded++; total_loaded++;
                }
            }
        }
        fclose(fp_nav);
        printf("[NDNav] Loaded %d navaids from earth_nav.dat\n", nav_loaded);
    } else {
        fprintf(stderr, "[NDNav] Cannot open %s: %s\n", nav_path, strerror(errno));
    }

    waypoint_total_count = total_loaded;
    printf("[NDNav] Total: %d waypoints loaded\n", total_loaded);
    return total_loaded > 0 ? 0 : -1;
}

// ============================================================
//  查询结果清理
// ============================================================

static void clean_result() {
    if (wp_result) {
        if (wp_result->data) {
            free(wp_result->data);
            wp_result->data = nullptr;
        }
        wp_result->count = 0;
        wp_result->index = 0;
    }
    wp_result_total = 0;
}

// ============================================================
//  内存释放
// ============================================================

void free_nav_data() {
    clean_result();

    if (wp_hash_table) {
        for (int i = 0; i < wp_hash_table->bucket_size; i++) {
            HashNode* curr = wp_hash_table->buckets[i];
            while (curr) {
                HashNode* next = curr->next;
                if (curr->wp_list) free(curr->wp_list);
                free(curr);
                curr = next;
            }
            wp_hash_table->buckets[i] = nullptr;
        }
        free(wp_hash_table->buckets);
        wp_hash_table->buckets = nullptr;
        free(wp_hash_table);
        wp_hash_table = nullptr;
    }

    waypoint_total_count = 0;
    wp_result_total = 0;
    printf("[NDNav] All nav data memory released\n");
}

// ============================================================
//  附近范围查询 (148km, 航向优先网格排序)
// ============================================================

int filter_waypoint_within_148km_ht(double target_lat, double target_lon, float heading) {
    clean_result();

    if (!wp_hash_table) {
        fprintf(stderr, "[NDNav] Hash table not initialized!\n");
        return -1;
    }

    // 初始化全局结果结构体
    if (!wp_result) {
        wp_result = (WAYPOINT_RESULT*)malloc(sizeof(WAYPOINT_RESULT));
        if (!wp_result) return -1;
        wp_result->count = 0;
        wp_result->index = 0;
        wp_result->data = nullptr;
    }

    wp_result->data = (NAV_WAYPOINT*)malloc(sizeof(NAV_WAYPOINT) * MAX_FILTER_RESULT_SIZE);
    if (!wp_result->data) return -1;
    memset(wp_result->data, 0, sizeof(NAV_WAYPOINT) * MAX_FILTER_RESULT_SIZE);

    // 动态计算网格偏移量 (纬度自适应)
    double lat_rad = fabs(target_lat) * M_PI / 180.0;
    double lon_km_per_degree = 111.0 * cos(lat_rad);
    double required_lon_offset = FILTER_DISTANCE_KM / lon_km_per_degree;
    int grid_offset = (int)ceil(required_lon_offset / 2.0);
    if (grid_offset < 1) grid_offset = 1;
    const int MAX_GRID_OFFSET = 10;
    if (grid_offset > MAX_GRID_OFFSET) grid_offset = MAX_GRID_OFFSET;

    int target_lat_grid = (int)(target_lat / GRID_SIZE);
    int target_lon_grid = (int)(target_lon / GRID_SIZE);
    int total_scanned = 0;
    int valid_count = 0;

    // 航向单位向量 (0°=正北)
    double heading_rad = heading * M_PI / 180.0;
    double dir_lat = cos(heading_rad);
    double dir_lon = sin(heading_rad);

    // 收集所有网格偏移 → 按航向优先级排序
    typedef struct { int lat_off; int lon_off; double priority; } GridOff;
    int grid_count = (2 * grid_offset + 1) * (2 * grid_offset + 1);
    GridOff* grid_list = (GridOff*)malloc(sizeof(GridOff) * grid_count);
    if (!grid_list) { free(wp_result->data); wp_result->data = nullptr; return -1; }

    int idx = 0;
    for (int lo = -grid_offset; lo <= grid_offset; lo++) {
        for (int la = -grid_offset; la <= grid_offset; la++) {
            grid_list[idx].lat_off = la;
            grid_list[idx].lon_off = lo;
            // 前方网格优先级高 (负值 → 排序靠前)
            grid_list[idx].priority = -(la * GRID_SIZE * dir_lat + lo * GRID_SIZE * dir_lon);
            idx++;
        }
    }

    // 冒泡排序 (按 priority 升序)
    for (int i = 0; i < grid_count - 1; i++) {
        for (int j = i + 1; j < grid_count; j++) {
            if (grid_list[i].priority > grid_list[j].priority) {
                GridOff t = grid_list[i]; grid_list[i] = grid_list[j]; grid_list[j] = t;
            }
        }
    }

    // 按优先级搜索网格
    for (int g = 0; g < grid_count && valid_count < MAX_FILTER_RESULT_SIZE; g++) {
        int curr_lat = target_lat_grid + grid_list[g].lat_off;
        int curr_lon = target_lon_grid + grid_list[g].lon_off;
        char grid_key[20] = {0};
        snprintf(grid_key, sizeof(grid_key), "%d_%d", curr_lat, curr_lon);

        unsigned int bucket = wp_ht_hash(grid_key, wp_hash_table->bucket_size);
        HashNode* node = wp_hash_table->buckets[bucket];

        while (node) {
            if (strcmp(node->grid_key, grid_key) == 0) {
                total_scanned += node->wp_count;
                for (int i = 0; i < node->wp_count && valid_count < MAX_FILTER_RESULT_SIZE; i++) {
                    NAV_WAYPOINT* wp = &node->wp_list[i];
                    double dist = calculate_distance_km(target_lat, target_lon, wp->lat, wp->lon);
                    if (dist <= FILTER_DISTANCE_KM) {
                        wp->distance = dist;
                        memcpy(&wp_result->data[valid_count], wp, sizeof(NAV_WAYPOINT));
                        valid_count++;
                    }
                }
                break;
            }
            node = node->next;
        }
    }

    free(grid_list);

    wp_result->count = valid_count;
    wp_result->index = valid_count;
    wp_result_total  = valid_count;

    printf("[NDNav] Filter: scanned=%d valid=%d (%.1fkm radius)\n",
           total_scanned, valid_count, FILTER_DISTANCE_KM);
    return valid_count;
}
