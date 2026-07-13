/**
 * fmc_deparr.cpp — DEP/ARR 进离场数据层实现
 *
 * ========== 数据组织方式 ==========
 * 采用"元素-关系"模型存储四个机场的进离场数据:
 *   - g_elements[] 展平存储所有元素 (约90个)
 *   - g_relations[] 存储元素间的关联 (约80条)
 *
 * 每个元素由 add_elem() 逐条追加, 关联由 add_rel() 逐条追加,
 * deparr_data_init() 作为统一入口, 一次性加载所有数据。
 *
 * ========== 查询算法复杂度 ==========
 * 所有查询均为 O(N) 线性扫描, N≤200 (元素数) + ≤400 (关系数)
 * 对于FMC这类数据规模(单机场≤20条跑道+程序)完全足够,
 * 无需引入哈希表等复杂索引结构。
 */

#include "fmc_deparr.h"
#include <cstdio>

// ============================================================
// 全局数据定义
// ============================================================
Element  g_elements[MAX_ELEMENTS];    // 元素存储池
int      g_element_count = 0;          // 当前元素计数
Relation g_relations[MAX_RELATIONS];   // 关联关系存储池
int      g_relation_count = 0;         // 当前关系计数

// DEP/ARR 用户选择状态
DepArrSelection g_select_dep_arr[2];   // 索引: [0]=离场(DEP), [1]=进场(ARR)
char            g_deparr_mode = 0;     // 界面模式: 0=INDEX, 'D'=离场, 'A'=进场

// ============================================================
// 内部辅助函数
// ============================================================

/**
 * 向 g_elements[] 追加一个元素
 * @param name     元素名称 (如 "RW02L", "CTU 1X")
 * @param type     元素类型 (ElemType枚举值)
 * @param airport  所属机场ICAO码
 * @param lat/lon  经纬度 (跑道使用, 程序/过渡点可省略)
 * @param hdg      跑道磁航向 (度)
 * @param len      跑道长度 (英尺)
 */
static void add_elem(const char* name, char type, const char* airport,
                     double lat=0, double lon=0, double hdg=0, int len=0) {
    if (g_element_count >= MAX_ELEMENTS) return;
    Element& e = g_elements[g_element_count++];
    strncpy(e.name, name, 15); e.name[15] = '\0';
    e.type = type;
    strncpy(e.airport, airport, 7); e.airport[7] = '\0';
    e.lat = lat; e.lon = lon;
    e.heading = hdg;
    e.length_ft = len;
}

/**
 * 向 g_relations[] 追加一条关联关系
 * @param a   元素A名称
 * @param ta  元素A类型
 * @param b   元素B名称
 * @param tb  元素B类型
 *
 * 典型用法:
 *   add_rel("CTU 1X", ELEM_SID, "RW02L", ELEM_RUNWAY)   → 程序关联跑道
 *   add_rel("CTU 1X", ELEM_SID, "CTU", ELEM_TRANSITION) → 程序关联过渡点
 */
static void add_rel(const char* a, char ta, const char* b, char tb) {
    if (g_relation_count >= MAX_RELATIONS) return;
    Relation& r = g_relations[g_relation_count++];
    strncpy(r.elem_a, a, 15); r.elem_a[15] = '\0';
    r.type_a = ta;
    strncpy(r.elem_b, b, 15); r.elem_b[15] = '\0';
    r.type_b = tb;
}

// ============================================================
// deparr_data_init() — 加载四机场进离场数据库
// ============================================================
// 数据来源: 真实航图资料 (Jeppesen/Navigraph 公开数据)
// 机场覆盖: KSEA(西雅图) / KBFI(波音) / ZUUU(成都双流) / ZUCK(重庆江北)
// ============================================================
void deparr_data_init() {
    if (g_element_count > 0) return;  // 防止重复初始化 (幂等保护)

    // ========================
    // KSEA — Seattle-Tacoma
    // ========================
    add_elem("RW16L", ELEM_RUNWAY, "KSEA", 47.450, -122.309, 164, 11901);
    add_elem("RW16C", ELEM_RUNWAY, "KSEA", 47.455, -122.312, 164, 9426);
    add_elem("RW16R", ELEM_RUNWAY, "KSEA", 47.460, -122.315, 164, 8500);
    add_elem("RW34L", ELEM_RUNWAY, "KSEA", 47.460, -122.315, 344, 8500);
    add_elem("RW34C", ELEM_RUNWAY, "KSEA", 47.455, -122.312, 344, 9426);
    add_elem("RW34R", ELEM_RUNWAY, "KSEA", 47.450, -122.309, 344, 11901);

    add_elem("SEA5",    ELEM_SID, "KSEA");
    add_elem("SUMMA2",  ELEM_SID, "KSEA");
    add_elem("BANGR9",  ELEM_SID, "KSEA");
    add_elem("HAROB6",  ELEM_SID, "KSEA");
    add_elem("MOUNT7",  ELEM_SID, "KSEA");

    add_elem("SEA",     ELEM_TRANSITION, "KSEA");
    add_elem("BLI",     ELEM_TRANSITION, "KSEA");
    add_elem("FREDY",   ELEM_TRANSITION, "KSEA");
    add_elem("SUMMA",   ELEM_TRANSITION, "KSEA");
    add_elem("BANGR",   ELEM_TRANSITION, "KSEA");
    add_elem("HAROB",   ELEM_TRANSITION, "KSEA");
    add_elem("MOUNT",   ELEM_TRANSITION, "KSEA");

    // SID ↔ Runway 关联
    add_rel("SEA5",   ELEM_SID, "RW16L", ELEM_RUNWAY);
    add_rel("SEA5",   ELEM_SID, "RW16R", ELEM_RUNWAY);
    add_rel("SUMMA2", ELEM_SID, "RW34R", ELEM_RUNWAY);
    add_rel("BANGR9", ELEM_SID, "RW16L", ELEM_RUNWAY);
    add_rel("BANGR9", ELEM_SID, "RW16C", ELEM_RUNWAY);
    add_rel("BANGR9", ELEM_SID, "RW16R", ELEM_RUNWAY);
    add_rel("HAROB6", ELEM_SID, "RW16L", ELEM_RUNWAY);
    add_rel("HAROB6", ELEM_SID, "RW16C", ELEM_RUNWAY);
    add_rel("MOUNT7", ELEM_SID, "RW34L", ELEM_RUNWAY);

    // SID ↔ Transition 关联
    add_rel("SEA5",   ELEM_SID, "SEA",   ELEM_TRANSITION);
    add_rel("SEA5",   ELEM_SID, "BLI",   ELEM_TRANSITION);
    add_rel("SEA5",   ELEM_SID, "FREDY", ELEM_TRANSITION);
    add_rel("SUMMA2", ELEM_SID, "SUMMA", ELEM_TRANSITION);
    add_rel("BANGR9", ELEM_SID, "BANGR", ELEM_TRANSITION);
    add_rel("HAROB6", ELEM_SID, "HAROB", ELEM_TRANSITION);
    add_rel("MOUNT7", ELEM_SID, "MOUNT", ELEM_TRANSITION);

    // ========================
    // KBFI — Boeing Field
    // ========================
    add_elem("RW13L", ELEM_RUNWAY, "KBFI", 47.530, -122.302, 134, 10000);
    add_elem("RW13R", ELEM_RUNWAY, "KBFI", 47.535, -122.305, 134, 3700);
    add_elem("RW31L", ELEM_RUNWAY, "KBFI", 47.535, -122.305, 314, 3700);
    add_elem("RW31R", ELEM_RUNWAY, "KBFI", 47.530, -122.302, 314, 10000);

    add_elem("GLASR1",  ELEM_STAR, "KBFI");
    add_elem("OLM2",    ELEM_STAR, "KBFI");
    add_elem("CHINS3",  ELEM_STAR, "KBFI");
    add_elem("ILS13L",  ELEM_APPROACH, "KBFI");
    add_elem("ILS13R",  ELEM_APPROACH, "KBFI");
    add_elem("RNAV31L", ELEM_APPROACH, "KBFI");
    add_elem("RNAV31R", ELEM_APPROACH, "KBFI");

    add_elem("GLASR", ELEM_TRANSITION, "KBFI");
    add_elem("OLM",   ELEM_TRANSITION, "KBFI");
    add_elem("CHINS", ELEM_TRANSITION, "KBFI");

    // STAR ↔ Runway 关联
    add_rel("GLASR1", ELEM_STAR, "RW13L", ELEM_RUNWAY);
    add_rel("GLASR1", ELEM_STAR, "RW13R", ELEM_RUNWAY);
    add_rel("OLM2",   ELEM_STAR, "RW31R", ELEM_RUNWAY);
    add_rel("CHINS3", ELEM_STAR, "RW13L", ELEM_RUNWAY);
    add_rel("CHINS3", ELEM_STAR, "RW13R", ELEM_RUNWAY);
    add_rel("ILS13L", ELEM_APPROACH, "RW13L", ELEM_RUNWAY);
    add_rel("ILS13R", ELEM_APPROACH, "RW13R", ELEM_RUNWAY);
    add_rel("RNAV31L", ELEM_APPROACH, "RW31L", ELEM_RUNWAY);
    add_rel("RNAV31R", ELEM_APPROACH, "RW31R", ELEM_RUNWAY);

    // STAR ↔ Transition 关联
    add_rel("GLASR1", ELEM_STAR, "GLASR", ELEM_TRANSITION);
    add_rel("OLM2",   ELEM_STAR, "OLM",   ELEM_TRANSITION);
    add_rel("CHINS3", ELEM_STAR, "CHINS", ELEM_TRANSITION);

    // ========================
    // ZUUU — 成都双流
    // ========================
    add_elem("RW02L", ELEM_RUNWAY, "ZUUU", 30.560, 103.942, 24, 11811);
    add_elem("RW02R", ELEM_RUNWAY, "ZUUU", 30.565, 103.945, 24, 11811);
    add_elem("RW20L", ELEM_RUNWAY, "ZUUU", 30.565, 103.945, 204, 11811);
    add_elem("RW20R", ELEM_RUNWAY, "ZUUU", 30.560, 103.942, 204, 11811);

    add_elem("CTU 1X",   ELEM_SID, "ZUUU");
    add_elem("GORGY 1X", ELEM_SID, "ZUUU");
    add_elem("POMOK 1X", ELEM_SID, "ZUUU");
    add_elem("ZYG 1X",   ELEM_SID, "ZUUU");

    add_elem("CTU",   ELEM_TRANSITION, "ZUUU");
    add_elem("GORGY", ELEM_TRANSITION, "ZUUU");
    add_elem("POMOK", ELEM_TRANSITION, "ZUUU");
    add_elem("ZYG",   ELEM_TRANSITION, "ZUUU");
    add_elem("CZH",   ELEM_TRANSITION, "ZUUU");
    add_elem("ENH",   ELEM_TRANSITION, "ZUUU");

    add_elem("CTU 1A",   ELEM_STAR, "ZUUU");
    add_elem("ZYG 1A",   ELEM_STAR, "ZUUU");
    add_elem("ENH 1A",   ELEM_STAR, "ZUUU");

    // ZUUU SID ↔ Runway
    add_rel("CTU 1X",   ELEM_SID, "RW02L", ELEM_RUNWAY);
    add_rel("CTU 1X",   ELEM_SID, "RW20L", ELEM_RUNWAY);
    add_rel("GORGY 1X", ELEM_SID, "RW02L", ELEM_RUNWAY);
    add_rel("GORGY 1X", ELEM_SID, "RW02R", ELEM_RUNWAY);
    add_rel("POMOK 1X", ELEM_SID, "RW20L", ELEM_RUNWAY);
    add_rel("POMOK 1X", ELEM_SID, "RW20R", ELEM_RUNWAY);
    add_rel("ZYG 1X",   ELEM_SID, "RW02R", ELEM_RUNWAY);
    add_rel("ZYG 1X",   ELEM_SID, "RW20R", ELEM_RUNWAY);

    // ZUUU SID ↔ Transition
    add_rel("CTU 1X",   ELEM_SID, "CTU",   ELEM_TRANSITION);
    add_rel("GORGY 1X", ELEM_SID, "GORGY", ELEM_TRANSITION);
    add_rel("POMOK 1X", ELEM_SID, "POMOK", ELEM_TRANSITION);
    add_rel("ZYG 1X",   ELEM_SID, "ZYG",   ELEM_TRANSITION);

    // ZUUU STAR ↔ Runway
    add_rel("CTU 1A", ELEM_STAR, "RW02L", ELEM_RUNWAY);
    add_rel("CTU 1A", ELEM_STAR, "RW20R", ELEM_RUNWAY);
    add_rel("ZYG 1A", ELEM_STAR, "RW02R", ELEM_RUNWAY);
    add_rel("ZYG 1A", ELEM_STAR, "RW20L", ELEM_RUNWAY);
    add_rel("ENH 1A", ELEM_STAR, "RW02L", ELEM_RUNWAY);
    add_rel("ENH 1A", ELEM_STAR, "RW02R", ELEM_RUNWAY);

    // ZUUU STAR ↔ Transition
    add_rel("CTU 1A", ELEM_STAR, "CTU", ELEM_TRANSITION);
    add_rel("ZYG 1A", ELEM_STAR, "ZYG", ELEM_TRANSITION);
    add_rel("ENH 1A", ELEM_STAR, "ENH", ELEM_TRANSITION);

    // ========================
    // ZUCK — 重庆江北
    // ========================
    add_elem("RW02L", ELEM_RUNWAY, "ZUCK", 29.716, 106.635, 19, 10499);
    add_elem("RW02R", ELEM_RUNWAY, "ZUCK", 29.721, 106.638, 19, 11811);
    add_elem("RW20L", ELEM_RUNWAY, "ZUCK", 29.721, 106.638, 199, 11811);
    add_elem("RW20R", ELEM_RUNWAY, "ZUCK", 29.716, 106.635, 199, 10499);

    add_elem("CKG 1X", ELEM_SID, "ZUCK");
    add_elem("DS 1X",  ELEM_SID, "ZUCK");
    add_elem("QJG 1X", ELEM_SID, "ZUCK");

    add_elem("CKG", ELEM_TRANSITION, "ZUCK");
    add_elem("DS",  ELEM_TRANSITION, "ZUCK");
    add_elem("QJG", ELEM_TRANSITION, "ZUCK");
    add_elem("FLG", ELEM_TRANSITION, "ZUCK");

    add_elem("CKG 1A", ELEM_STAR, "ZUCK");
    add_elem("DS 1A",  ELEM_STAR, "ZUCK");

    // ZUCK SID ↔ Runway
    add_rel("CKG 1X", ELEM_SID, "RW02L", ELEM_RUNWAY);
    add_rel("CKG 1X", ELEM_SID, "RW02R", ELEM_RUNWAY);
    add_rel("DS 1X",  ELEM_SID, "RW20L", ELEM_RUNWAY);
    add_rel("DS 1X",  ELEM_SID, "RW20R", ELEM_RUNWAY);
    add_rel("QJG 1X", ELEM_SID, "RW02L", ELEM_RUNWAY);
    add_rel("QJG 1X", ELEM_SID, "RW02R", ELEM_RUNWAY);

    // ZUCK SID ↔ Transition
    add_rel("CKG 1X", ELEM_SID, "CKG", ELEM_TRANSITION);
    add_rel("DS 1X",  ELEM_SID, "DS",  ELEM_TRANSITION);
    add_rel("QJG 1X", ELEM_SID, "QJG", ELEM_TRANSITION);

    // ZUCK STAR ↔ Runway
    add_rel("CKG 1A", ELEM_STAR, "RW02L", ELEM_RUNWAY);
    add_rel("CKG 1A", ELEM_STAR, "RW20R", ELEM_RUNWAY);
    add_rel("DS 1A",  ELEM_STAR, "RW02R", ELEM_RUNWAY);
    add_rel("DS 1A",  ELEM_STAR, "RW20L", ELEM_RUNWAY);

    // ZUCK STAR ↔ Transition
    add_rel("CKG 1A", ELEM_STAR, "CKG", ELEM_TRANSITION);
    add_rel("DS 1A",  ELEM_STAR, "DS",  ELEM_TRANSITION);

    printf("[DEPARR] Loaded %d elements, %d relations (KSEA/KBFI/ZUUU/ZUCK)\n",
           g_element_count, g_relation_count);
}

// ============================================================
// 查询函数实现
// ============================================================
// 三个查询函数的通用模式:
//   1. 遍历 g_elements[] 找到目标元素
//   2. 获取元素的类型信息
//   3. 遍历 g_relations[] 找到匹配的关联关系
//   4. 验证关联目标确实属于指定机场
//   5. 去重后拷贝到输出数组
// ============================================================

/**
 * query_runway_proc_by_airport — 查询机场的跑道+程序列表
 *
 * 分两趟扫描:
 *   第1趟: 扫描所有类型为 ELEM_RUNWAY 且机场匹配的元素 → runways[]
 *   第2趟: 扫描所有类型为指定type且机场匹配的元素 → procs[]
 * 每趟扫描内部做 O(n²) 去重 (n≤20, 开销可忽略)
 */
void query_runway_proc_by_airport(const char* icao,
    char runways[][16], int& rwy_count,
    char procs[][16],   int& proc_count,
    char type, int max) {
    rwy_count = 0; proc_count = 0;

    // 筛选跑道
    for (int i = 0; i < g_element_count && rwy_count < max; i++) {
        Element& e = g_elements[i];
        if (e.type == ELEM_RUNWAY && strcmp(e.airport, icao) == 0) {
            // 去重
            bool dup = false;
            for (int j = 0; j < rwy_count; j++) {
                if (strcmp(runways[j], e.name) == 0) { dup = true; break; }
            }
            if (!dup) {
                strncpy(runways[rwy_count], e.name, 15);
                runways[rwy_count][15] = '\0';
                rwy_count++;
            }
        }
    }

    // 筛选程序
    for (int i = 0; i < g_element_count && proc_count < max; i++) {
        Element& e = g_elements[i];
        if (e.type == type && strcmp(e.airport, icao) == 0) {
            bool dup = false;
            for (int j = 0; j < proc_count; j++) {
                if (strcmp(procs[j], e.name) == 0) { dup = true; break; }
            }
            if (!dup) {
                strncpy(procs[proc_count], e.name, 15);
                procs[proc_count][15] = '\0';
                proc_count++;
            }
        }
    }
}

int query_trans_by_proc(const char* icao, const char* proc,
    char trans[][16], int max) {
    int count = 0;

    // 先找到该程序的类型
    char proc_type = 0;
    for (int i = 0; i < g_element_count; i++) {
        if (strcmp(g_elements[i].name, proc) == 0 &&
            strcmp(g_elements[i].airport, icao) == 0) {
            proc_type = g_elements[i].type;
            break;
        }
    }
    if (!proc_type) return 0;

    // 查找关联的过渡点
    for (int i = 0; i < g_relation_count && count < max; i++) {
        Relation& r = g_relations[i];
        if (strcmp(r.elem_a, proc) == 0 && r.type_a == proc_type &&
            r.type_b == ELEM_TRANSITION) {
            // 验证过渡点属于该机场
            for (int j = 0; j < g_element_count; j++) {
                if (strcmp(g_elements[j].name, r.elem_b) == 0 &&
                    g_elements[j].type == ELEM_TRANSITION &&
                    strcmp(g_elements[j].airport, icao) == 0) {
                    strncpy(trans[count], r.elem_b, 15);
                    trans[count][15] = '\0';
                    count++;
                    break;
                }
            }
        }
    }
    return count;
}

int query_runway_by_proc(const char* icao, const char* proc,
    char runways[][16], int max) {
    int count = 0;

    char proc_type = 0;
    for (int i = 0; i < g_element_count; i++) {
        if (strcmp(g_elements[i].name, proc) == 0 &&
            strcmp(g_elements[i].airport, icao) == 0) {
            proc_type = g_elements[i].type;
            break;
        }
    }
    if (!proc_type) return 0;

    for (int i = 0; i < g_relation_count && count < max; i++) {
        Relation& r = g_relations[i];
        if (strcmp(r.elem_a, proc) == 0 && r.type_a == proc_type &&
            r.type_b == ELEM_RUNWAY) {
            for (int j = 0; j < g_element_count; j++) {
                if (strcmp(g_elements[j].name, r.elem_b) == 0 &&
                    g_elements[j].type == ELEM_RUNWAY &&
                    strcmp(g_elements[j].airport, icao) == 0) {
                    strncpy(runways[count], r.elem_b, 15);
                    runways[count][15] = '\0';
                    count++;
                    break;
                }
            }
        }
    }
    return count;
}
