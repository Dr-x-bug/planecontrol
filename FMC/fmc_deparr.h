/**
 * fmc_deparr.h — DEP/ARR 进离场程序数据层
 *
 * 按 PPT Day09 规范设计:
 *   通用元素结构体 Element: 名称 + 类型 + 所属机场
 *   关联关系结构体 Relation: 两个元素的关联
 *   全局数组 g_elements[] / g_relations[]
 *   查询函数: query_runway_proc_by_airport / query_trans_by_proc / query_runway_by_proc
 */

#pragma once
#include <cstring>

// ===== 元素类型 =====
enum ElemType {
    ELEM_RUNWAY      = 'R',   // 跑道
    ELEM_SID         = 'S',   // 离场程序 (SID)
    ELEM_STAR        = 'T',   // 进场程序 (STAR)
    ELEM_APPROACH    = 'A',   // 进近程序
    ELEM_TRANSITION  = 'X',   // 过渡点
};

// ===== 通用元素结构体 (名称 + 类型 + 所属机场) =====
struct Element {
    char name[16];       // 标识符 (如 "RW02L", "CTU 1X")
    char type;           // ElemType 枚举值
    char airport[8];     // 所属机场ICAO (如 "ZUUU")
    double lat, lon;     // 经纬度 (跑道有用)
    double heading;      // 跑道航向
    int    length_ft;    // 跑道长度
};

// ===== 关联关系结构体 =====
struct Relation {
    char elem_a[16];     // 元素A名称
    char elem_b[16];     // 元素B名称
    char type_a;         // 元素A类型
    char type_b;         // 元素B类型
};

// ===== 全局数据数组 =====
#define MAX_ELEMENTS   200
#define MAX_RELATIONS  400

extern Element  g_elements[MAX_ELEMENTS];
extern int      g_element_count;
extern Relation g_relations[MAX_RELATIONS];
extern int      g_relation_count;

// ===== 查询函数 =====

/** 查询某机场的所有跑道和程序 (SID/STAR)
 *  @param icao    机场ICAO码
 *  @param runways 输出跑道名称数组 (已分配空间)
 *  @param procs   输出程序名称数组 (已分配空间)
 *  @param type    程序类型: ELEM_SID 或 ELEM_STAR
 *  @param max     每个数组最大容量
 *  @return        分别返回跑道数和程序数 (通过引用参数)
 */
void query_runway_proc_by_airport(const char* icao,
    char runways[][16], int& rwy_count,
    char procs[][16],   int& proc_count,
    char type, int max);

/** 查询某程序的过渡点列表
 *  @param icao    机场ICAO
 *  @param proc    程序名称
 *  @param trans   输出过渡点名称数组
 *  @param max     最大容量
 *  @return        过渡点数量
 */
int  query_trans_by_proc(const char* icao, const char* proc,
    char trans[][16], int max);

/** 查询某程序关联的跑道
 *  @param icao    机场ICAO
 *  @param proc    程序名称
 *  @param runways 输出跑道名称数组
 *  @param max     最大容量
 *  @return        关联跑道数量
 */
int  query_runway_by_proc(const char* icao, const char* proc,
    char runways[][16], int max);

// ===== DEP/ARR 选择状态 =====

/** 单个离场/进场选择数据结构 */
struct DepArrSelection {
    char select_runway[16];    // 选中跑道
    char select_proc[16];      // 选中程序
    char select_trans[16];     // 选中过渡点

    // 候选列表 (由查询函数填充)
    char runways[20][16];
    int  rwy_count;
    char procs[20][16];
    int  proc_count;
    char trans[20][16];
    int  trans_count;

    // 选择阶段: 0=选程序, 1=选跑道, 2=选过渡点
    int  step;

    DepArrSelection() { clear(); }
    void clear() {
        memset(this, 0, sizeof(*this));
        step = 0;
    }
};

// ===== 全局 DEP/ARR 状态 =====
// select_dep_arr[0] = 离场(DEP), select_dep_arr[1] = 进场(ARR)
extern DepArrSelection g_select_dep_arr[2];

// DEP/ARR 模式: 0=INDEX首页, 'D'=离场, 'A'=进场
extern char g_deparr_mode;

// ===== 数据初始化 =====
/** 加载所有机场的进离场数据到全局数组 */
void deparr_data_init();

