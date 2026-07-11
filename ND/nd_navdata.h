/**
 * nd_navdata.h — ND 导航数据层 (航点/机场/塔台)
 * 职责: 网格化哈希表实现导航数据高效加载与附近范围查询
 * 核心: 1°网格划分 + DJB2哈希 + 链表法解决冲突 + 航向优先排序搜索
 */
#ifndef ND_NAVDATA_H
#define ND_NAVDATA_H

#include "renderer.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "xplaneConnect.h"
#ifdef __cplusplus
}
#endif

// ============================================================
//  常量定义
// ============================================================

#define MAX_TOTAL_WAYPOINTS    240000      // 固定总容量
#define HASH_BUCKET_SIZE       1009        // 哈希桶数量（质数）
#define GRID_SIZE              1.0         // 网格大小 1.0°
#define DATA_ROOT_PATH         "../assets/" // 数据文件根路径
#define EARTH_RADIUS_KM        6371.0      // 地球平均半径 (km)
#define FILTER_DISTANCE_KM     148.0       // 查询范围 148km ≈ 80nm
#define MAX_FILTER_RESULT_SIZE 30          // 最大结果数

// ============================================================
//  一、ND 飞行数据结构体 (X-Plane 数据)
// ============================================================

typedef struct {
    double latitude;         // 纬度
    double longitude;        // 经度
    float  ground_speed;     // 地速 (kts)
    float  true_air_speed;   // 真空速 (kts)
    float  heading;          // 磁航向 (degrees)
} NDNavData;

// ============================================================
//  二、航路点结构体 (扩展距离字段)
// ============================================================

typedef struct {
    int    num;              // 类型: 1=航路点, 3=VOR, 12/13=VOR/DME 等
    double lat;              // 纬度
    double lon;              // 经度
    char   name[20];         // 名称
    double distance;         // 计算的距离（临时存储）
} NAV_WAYPOINT;

// ============================================================
//  三、哈希表节点 (单个网格的航点链表)
// ============================================================

typedef struct HashNode {
    char          grid_key[20];   // 网格键 (如 "33_9")
    NAV_WAYPOINT* wp_list;        // 该网格的航点数组
    int           wp_count;       // 航点数量
    int           wp_capacity;    // 数组容量（动态扩容）
    struct HashNode* next;        // 链表法解决哈希冲突
} HashNode;

// ============================================================
//  四、哈希表主结构
// ============================================================

typedef struct {
    HashNode** buckets;       // 哈希桶数组
    int        bucket_size;   // 桶数量
} WaypointHashTable;

// ============================================================
//  五、动态查询结果
// ============================================================

typedef struct {
    NAV_WAYPOINT* data;       // 结果数组
    int           index;      // 当前索引
    int           count;      // 符合条件条数
} WAYPOINT_RESULT;

// ============================================================
//  全局变量声明
// ============================================================

extern int               waypoint_total_count;
extern WaypointHashTable* wp_hash_table;
extern int               wp_result_total;
extern WAYPOINT_RESULT*  wp_result;

// ============================================================
//  函数声明
// ============================================================

// X-Plane 数据获取
int getNDNavData(XPCSocket sock, NDNavData* data);

// 数据加载 / 清理
int  load_all_nav_data();
void free_nav_data();

// 附近范围查询 (148km, 带航向优先级)
int filter_waypoint_within_148km_ht(double target_lat, double target_lon, float heading);

// 距离计算
double calculate_distance_km(double lat1, double lon1, double lat2, double lon2);

#endif // ND_NAVDATA_H
