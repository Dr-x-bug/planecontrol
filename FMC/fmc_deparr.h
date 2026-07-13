/**
 * fmc_deparr.h — DEP/ARR 进离场程序数据层
 *
 * ========== 设计原理 (PPT Day09 Slide 12-15) ==========
 * 商用客机的进离场程序涉及三个核心概念:
 *   1. 跑道 (Runway)       — 飞机起降的物理跑道
 *   2. 离场/进场程序 (SID/STAR) — 标准仪表离场/进场航线
 *   3. 过渡点 (Transition) — 从程序航路过渡到航路网络的关键点
 *
 * 数据关系:
 *   机场 ─1:N─→ 跑道
 *   机场 ─1:N─→ 程序 (SID/STAR/APPROACH)
 *   程序 ─N:M─→ 跑道        (一个程序可关联多条跑道, 反之亦然)
 *   程序 ─1:N─→ 过渡点      (一个程序有0~N个过渡点)
 *
 * ========== 存储方案 ==========
 * 采用"通用元素 + 关联关系"的双数组设计:
 *   g_elements[]  — 扁平化存储所有元素(跑道/程序/过渡点)
 *   g_relations[] — 存储元素间的关联关系对
 *
 * 优点: 避免深层嵌套结构体, 查询通过遍历筛选实现, 简单直观
 *
 * ========== 使用流程 ==========
 * 1. deparr_data_init()          → 加载KSEA/KBFI/ZUUU/ZUCK四机场数据
 * 2. query_runway_proc_by_airport() → 获取某机场的跑道和程序列表
 * 3. query_trans_by_proc()        → 获取某程序的过渡点
 * 4. query_runway_by_proc()       → 获取某程序关联的跑道
 * 5. g_select_dep_arr[0/1]        → 存储用户选择的离场/进场配置
 */

#pragma once
#include <cstring>

// ============================================================
// 元素类型枚举
// ============================================================
enum ElemType {
    ELEM_RUNWAY      = 'R',   // 跑道 (Runway)
    ELEM_SID         = 'S',   // 标准仪表离场 (Standard Instrument Departure)
    ELEM_STAR        = 'T',   // 标准终端进场航线 (Standard Terminal Arrival Route)
    ELEM_APPROACH    = 'A',   // 进近程序 (Approach, 如ILS/RNAV)
    ELEM_TRANSITION  = 'X',   // 过渡点 (Transition, 连接程序和航路的中间点)
};

// ============================================================
// 通用元素结构体 (PPT Slide 12核心设计)
// ============================================================
// 设计要点: 所有类型的导航元素(跑道/程序/过渡点)共用同一个结构体,
//           通过 type 字段区分类型, 通过 airport 字段归属到具体机场
struct Element {
    char name[16];       // 标识符 (如 "RW02L", "CTU 1X", "GLASR")
    char type;           // ElemType 枚举值, 决定该元素的语义
    char airport[8];     // 所属机场ICAO码 (如 "ZUUU", "KSEA")
    double lat, lon;     // 经纬度坐标 (主要用于跑道定位)
    double heading;      // 跑道磁航向 (度, 如 164°=跑道16L)
    int    length_ft;    // 跑道长度 (英尺, 如 11901ft)
};

// ============================================================
// 关联关系结构体 (PPT Slide 12核心设计)
// ============================================================
// 表示两个元素之间的关联, 例如:
//   "CTU 1X" (SID) 关联 "RW02L" (Runway) — 离场程序CTU 1X可从跑道02L起飞
//   "CTU 1X" (SID) 关联 "CTU" (Transition) — 离场程序CTU 1X经过过渡点CTU
struct Relation {
    char elem_a[16];     // 元素A的名称标识
    char elem_b[16];     // 元素B的名称标识
    char type_a;         // 元素A的类型 (用于精确匹配, 避免同名不同类型混淆)
    char type_b;         // 元素B的类型
};

// ============================================================
// 全局数据存储
// ============================================================
#define MAX_ELEMENTS   200   // 最大元素容量 (四机场合计约90个)
#define MAX_RELATIONS  400   // 最大关联关系容量

extern Element  g_elements[MAX_ELEMENTS];   // 元素数组
extern int      g_element_count;            // 当前元素数量
extern Relation g_relations[MAX_RELATIONS]; // 关联关系数组
extern int      g_relation_count;           // 当前关联关系数量

// ============================================================
// 查询函数 (PPT Slide 14)
// ============================================================

/**
 * 查询某机场的所有跑道和程序
 *
 * 算法: 遍历 g_elements[], 筛选 type==ELEM_RUNWAY 且 airport==icao 的元素;
 *        再筛选 type==指定程序类型 且 airport==icao 的元素;
 *        对结果去重后拷贝到输出数组
 *
 * @param icao     机场ICAO码 (如 "ZUUU")
 * @param runways  输出: 跑道名称二维数组 (调用者预分配 max*16 字节)
 * @param rwy_count 输出: 实际跑道数量
 * @param procs    输出: 程序名称二维数组
 * @param proc_count 输出: 实际程序数量
 * @param type     程序类型筛选: ELEM_SID(离场) 或 ELEM_STAR(进场) 或 ELEM_APPROACH(进近)
 * @param max      每个输出数组的最大条目数
 */
void query_runway_proc_by_airport(const char* icao,
    char runways[][16], int& rwy_count,
    char procs[][16],   int& proc_count,
    char type, int max);

/**
 * 查询某程序的过渡点列表
 *
 * 算法: 先在 g_elements[] 中定位程序并获取其类型;
 *        然后在 g_relations[] 中查找 type_a==程序类型 且 type_b==ELEM_TRANSITION 的关联;
 *        验证过渡点确实属于该机场后输出
 *
 * @param icao     机场ICAO码
 * @param proc     程序名称 (如 "CTU 1X")
 * @param trans    输出: 过渡点名称数组
 * @param max      最大容量
 * @return         实际找到的过渡点数量
 */
int  query_trans_by_proc(const char* icao, const char* proc,
    char trans[][16], int max);

/**
 * 查询某程序关联的跑道
 *
 * 算法: 先在 g_elements[] 中定位程序获取类型;
 *        然后在 g_relations[] 中查找 type_b==ELEM_RUNWAY 的关联;
 *        验证跑道属于该机场后输出
 *
 * @param icao     机场ICAO码
 * @param proc     程序名称
 * @param runways  输出: 跑道名称数组
 * @param max      最大容量
 * @return         实际找到的关联跑道数量
 */
int  query_runway_by_proc(const char* icao, const char* proc,
    char runways[][16], int max);

// ============================================================
// DEP/ARR 用户选择状态 (PPT Slide 15)
// ============================================================

/**
 * 单个离场/进场的选择数据结构
 *
 * 生命周期:
 *   1. 用户点击 <DEP 或 <ARR → clear() 清空 → 查询填充候选列表
 *   2. 用户点击程序 → 更新 select_proc + 查询过渡点/跑道
 *   3. 用户点击跑道 → 更新 select_runway → 点亮EXEC灯
 *   4. 用户按EXEC → 确认选择 → 同步到航路系统
 */
struct DepArrSelection {
    // ---- 用户选中项 ----
    char select_runway[16];    // 用户选中的跑道 (如 "RW02L")
    char select_proc[16];      // 用户选中的程序 (如 "CTU 1X")
    char select_trans[16];     // 用户选中的过渡点 (如 "CTU")

    // ---- 候选列表 (由查询函数自动填充) ----
    char runways[20][16];      // 可选跑道列表
    int  rwy_count;            // 跑道数量
    char procs[20][16];        // 可选程序列表
    int  proc_count;           // 程序数量
    char trans[20][16];        // 可选过渡点列表
    int  trans_count;          // 过渡点数量

    // ---- 选择阶段 (预留, 当前未使用) ----
    int  step;                 // 0=选程序, 1=选跑道, 2=选过渡点

    DepArrSelection() { clear(); }
    void clear() {
        memset(this, 0, sizeof(*this));  // 全部字段归零
        step = 0;
    }
};

// ============================================================
// 全局 DEP/ARR 状态
// ============================================================

// 离场/进场选择数组: [0]=离场(DEP), [1]=进场(ARR)
// 二者数据结构相同, 通过数组下标区分语义
extern DepArrSelection g_select_dep_arr[2];

// DEP/ARR 界面模式标志:
//   0   = INDEX首页 (显示 <DEP / <ARR 入口)
//   'D' = 离场选择页 (显示SID列表 + 跑道列表)
//   'A' = 进场选择页 (显示STAR/APPROACH列表 + 跑道列表)
extern char g_deparr_mode;

// ============================================================
// 数据初始化
// ============================================================

/**
 * 加载所有预置机场的进离场数据到 g_elements[] 和 g_relations[]
 *
 * 内置机场: KSEA(西雅图) / KBFI(波音机场) / ZUUU(成都双流) / ZUCK(重庆江北)
 *
 * 注意: 该函数只能调用一次 (内部通过 g_element_count>0 判断是否已初始化)
 */
void deparr_data_init();

