/**
 * fmc_data.cpp — 进离场数据加载与查询实现
 *
 * 预置机场: KSEA / KBFI / ZUUU / ZUCK
 */

#include "fmc_data.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// 全局数据定义
DepArrElement g_deparr_elements[200];
DepArrRelation g_deparr_relations[2000];
int            g_deparr_element_count = 0;
int            g_deparr_relation_count = 0;

char** runway       = NULL;
char** proc         = NULL;
char** runway_trans = NULL;
char** proc_trans   = NULL;
int    runway_count = 0;
int    proc_count   = 0;
int    runway_trans_count = 0;
int    proc_trans_count   = 0;

SelectDepArr select_dep_arr[2];
int  dep_arr_index  = 1;
int  dep_arr_type   = 0;
char show_ariport[20] = {0};
int  will_exec      = 0;

// 添加元素
void add_element(const char* name, DepArrElemType type, const char* airport) {
    if (g_deparr_element_count >= 200) return;
    DepArrElement* elem = &g_deparr_elements[g_deparr_element_count++];
    strncpy(elem->name, name, 19);    elem->name[19] = '\0';
    elem->type = type;
    strncpy(elem->airport, airport, 19); elem->airport[19] = '\0';
}

// 添加关联关系
void add_relation(const char* airport, const char* elem1, DepArrElemType el1_type,
                  const char* elem2, DepArrElemType el2_type) {
    if (g_deparr_relation_count >= 2000) return;
    DepArrRelation* rel = &g_deparr_relations[g_deparr_relation_count++];
    strncpy(rel->airport, airport, 19); rel->airport[19] = '\0';
    strncpy(rel->elem1, elem1, 19);     rel->elem1[19] = '\0';
    rel->elem1_type = el1_type;
    strncpy(rel->elem2, elem2, 19);     rel->elem2[19] = '\0';
    rel->elem2_type = el2_type;
}

// 初始化展示列表内存
static void alloc_display_lists() {
    runway       = (char**)malloc(sizeof(char*) * 100);
    proc         = (char**)malloc(sizeof(char*) * 100);
    runway_trans = (char**)malloc(sizeof(char*) * 100);
    proc_trans   = (char**)malloc(sizeof(char*) * 100);
    for (int i = 0; i < 100; i++) {
        runway[i]       = (char*)malloc(20);
        proc[i]         = (char*)malloc(20);
        runway_trans[i] = (char*)malloc(20);
        proc_trans[i]   = (char*)malloc(20);
    }
}

// 初始化机场数据
void init_airport_data(void) {
    alloc_display_lists();

    // === KSEA ===
    add_element("RW16C", TYPE_RUNWAY, "KSEA");
    add_element("RW16L", TYPE_RUNWAY, "KSEA");
    add_element("RW34C", TYPE_RUNWAY, "KSEA");
    add_element("ATOM E2", TYPE_TAKEOFF_PROC, "KSEA");
    add_element("BANGR 9", TYPE_TAKEOFF_PROC, "KSEA");
    add_element("SUMMA 1", TYPE_TAKEOFF_PROC, "KSEA");
    add_element("COV",  TYPE_WAYPOINT, "KSEA");
    add_element("HBM",  TYPE_WAYPOINT, "KSEA");
    add_element("XYZ",  TYPE_WAYPOINT, "KSEA");

    add_relation("KSEA", "RW16C", TYPE_RUNWAY, "ATOM E2", TYPE_TAKEOFF_PROC);
    add_relation("KSEA", "RW16C", TYPE_RUNWAY, "SUMMA 1", TYPE_TAKEOFF_PROC);
    add_relation("KSEA", "RW16L", TYPE_RUNWAY, "BANGR 9", TYPE_TAKEOFF_PROC);
    add_relation("KSEA", "RW34C", TYPE_RUNWAY, "SUMMA 1", TYPE_TAKEOFF_PROC);
    add_relation("KSEA", "RW16C", TYPE_RUNWAY, "COV", TYPE_WAYPOINT);
    add_relation("KSEA", "RW16C", TYPE_RUNWAY, "HBM", TYPE_WAYPOINT);
    add_relation("KSEA", "RW34C", TYPE_RUNWAY, "XYZ", TYPE_WAYPOINT);
    add_relation("KSEA", "ATOM E2", TYPE_TAKEOFF_PROC, "COV", TYPE_WAYPOINT);
    add_relation("KSEA", "SUMMA 1", TYPE_TAKEOFF_PROC, "HBM", TYPE_WAYPOINT);

    // === KBFI ===
    add_element("RW14R", TYPE_RUNWAY, "KBFI");
    add_element("RW14L", TYPE_RUNWAY, "KBFI");
    add_element("RW32L", TYPE_RUNWAY, "KBFI");
    add_element("RW32R", TYPE_RUNWAY, "KBFI");
    add_element("CHINS 3", TYPE_TAKEOFF_PROC, "KBFI");
    add_element("EPH 8",   TYPE_TAKEOFF_PROC, "KBFI");
    add_element("HAROB 4", TYPE_TAKEOFF_PROC, "KBFI");
    add_element("JAKSN 5", TYPE_TAKEOFF_PROC, "KBFI");
    add_element("PAINE 2", TYPE_TAKEOFF_PROC, "KBFI");
    add_element("SUMMA 6", TYPE_TAKEOFF_PROC, "KBFI");

    add_relation("KBFI", "RW14R", TYPE_RUNWAY, "CHINS 3", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW14R", TYPE_RUNWAY, "HAROB 4", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW14L", TYPE_RUNWAY, "EPH 8",   TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW14L", TYPE_RUNWAY, "JAKSN 5", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW32L", TYPE_RUNWAY, "CHINS 3", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW32L", TYPE_RUNWAY, "PAINE 2", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW32R", TYPE_RUNWAY, "EPH 8",   TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW32R", TYPE_RUNWAY, "SUMMA 6", TYPE_TAKEOFF_PROC);

    // === ZUUU ===
    add_element("RW02L", TYPE_RUNWAY, "ZUUU");
    add_element("RW02R", TYPE_RUNWAY, "ZUUU");
    add_element("RW20L", TYPE_RUNWAY, "ZUUU");
    add_element("RW20R", TYPE_RUNWAY, "ZUUU");
    add_element("CTU 1X",   TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("GORGY 1X", TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("POMOK 1X", TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("ROBIG 1X", TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("XARPI 1X", TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("ZYU 1X",   TYPE_TAKEOFF_PROC, "ZUUU");

    add_relation("ZUUU", "RW02L", TYPE_RUNWAY, "CTU 1X",   TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW02L", TYPE_RUNWAY, "GORGY 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW02R", TYPE_RUNWAY, "POMOK 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW02R", TYPE_RUNWAY, "ROBIG 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW20L", TYPE_RUNWAY, "CTU 1X",   TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW20L", TYPE_RUNWAY, "XARPI 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW20R", TYPE_RUNWAY, "POMOK 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW20R", TYPE_RUNWAY, "ZYU 1X",   TYPE_TAKEOFF_PROC);

    // === ZUCK ===
    add_element("RW02",  TYPE_RUNWAY, "ZUCK");
    add_element("RW03",  TYPE_RUNWAY, "ZUCK");
    add_element("RW20",  TYPE_RUNWAY, "ZUCK");
    add_element("RW21",  TYPE_RUNWAY, "ZUCK");
    add_element("CKG 1X",   TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("DUGUB 1X", TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("GOVSA 1X", TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("IDLUN 1X", TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("MEKNO 1X", TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("NILOM 1X", TYPE_TAKEOFF_PROC, "ZUCK");

    add_relation("ZUCK", "RW02", TYPE_RUNWAY, "CKG 1X",   TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW02", TYPE_RUNWAY, "DUGUB 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW02", TYPE_RUNWAY, "GOVSA 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW03", TYPE_RUNWAY, "GOVSA 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW03", TYPE_RUNWAY, "IDLUN 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW03", TYPE_RUNWAY, "MEKNO 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW20", TYPE_RUNWAY, "CKG 1X",   TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW20", TYPE_RUNWAY, "DUGUB 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW20", TYPE_RUNWAY, "MEKNO 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW21", TYPE_RUNWAY, "GOVSA 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW21", TYPE_RUNWAY, "IDLUN 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW21", TYPE_RUNWAY, "NILOM 1X", TYPE_TAKEOFF_PROC);
}

// 销毁
void destroy_airport_data(void) {
    if (runway) {
        for (int i = 0; i < 100; i++) if (runway[i]) free(runway[i]);
        free(runway); runway = NULL;
    }
    if (proc) {
        for (int i = 0; i < 100; i++) if (proc[i]) free(proc[i]);
        free(proc); proc = NULL;
    }
    if (runway_trans) {
        for (int i = 0; i < 100; i++) if (runway_trans[i]) free(runway_trans[i]);
        free(runway_trans); runway_trans = NULL;
    }
    if (proc_trans) {
        for (int i = 0; i < 100; i++) if (proc_trans[i]) free(proc_trans[i]);
        free(proc_trans); proc_trans = NULL;
    }
    g_deparr_element_count = 0;
    g_deparr_relation_count = 0;
    runway_count = proc_count = runway_trans_count = proc_trans_count = 0;
}

// 查询机场对应的跑道和程序
int query_runway_proc_by_airport(const char* airport) {
    runway_count = 0;
    proc_count = 0;
    for (int i = 0; i < g_deparr_element_count; i++) {
        if (strcmp(g_deparr_elements[i].airport, airport) == 0) {
            if (g_deparr_elements[i].type == TYPE_RUNWAY && runway_count < 100) {
                strncpy(runway[runway_count], g_deparr_elements[i].name, 19);
                runway[runway_count][19] = '\0';
                runway_count++;
            } else if (g_deparr_elements[i].type == TYPE_TAKEOFF_PROC && proc_count < 100) {
                strncpy(proc[proc_count], g_deparr_elements[i].name, 19);
                proc[proc_count][19] = '\0';
                proc_count++;
            }
        }
    }
    return runway_count + proc_count;
}

// 按机场+跑道查询程序
int query_proc_by_runway(const char* airport, const char* rwy) {
    proc_count = 0;
    for (int i = 0; i < g_deparr_relation_count && proc_count < 100; i++) {
        if (strcmp(g_deparr_relations[i].airport, airport) != 0) continue;
        if (strcmp(g_deparr_relations[i].elem1, rwy) == 0 && g_deparr_relations[i].elem2_type == TYPE_TAKEOFF_PROC) {
            strncpy(proc[proc_count], g_deparr_relations[i].elem2, 19);
            proc[proc_count][19] = '\0';
            proc_count++;
        } else if (strcmp(g_deparr_relations[i].elem2, rwy) == 0 && g_deparr_relations[i].elem1_type == TYPE_TAKEOFF_PROC) {
            strncpy(proc[proc_count], g_deparr_relations[i].elem1, 19);
            proc[proc_count][19] = '\0';
            proc_count++;
        }
    }
    return proc_count;
}

// 按机场+跑道查询过渡点
int query_trans_by_runway(const char* airport, const char* rwy) {
    runway_trans_count = 0;
    for (int i = 0; i < g_deparr_relation_count && runway_trans_count < 100; i++) {
        if (strcmp(g_deparr_relations[i].airport, airport) != 0) continue;
        if (strcmp(g_deparr_relations[i].elem1, rwy) == 0 && g_deparr_relations[i].elem2_type == TYPE_WAYPOINT) {
            strncpy(runway_trans[runway_trans_count], g_deparr_relations[i].elem2, 19);
            runway_trans[runway_trans_count][19] = '\0';
            runway_trans_count++;
        } else if (strcmp(g_deparr_relations[i].elem2, rwy) == 0 && g_deparr_relations[i].elem1_type == TYPE_WAYPOINT) {
            strncpy(runway_trans[runway_trans_count], g_deparr_relations[i].elem1, 19);
            runway_trans[runway_trans_count][19] = '\0';
            runway_trans_count++;
        }
    }
    return runway_trans_count;
}

// 按机场+程序查询跑道
int query_runway_by_proc(const char* airport, const char* procedure) {
    runway_count = 0;
    for (int i = 0; i < g_deparr_relation_count && runway_count < 100; i++) {
        if (strcmp(g_deparr_relations[i].airport, airport) != 0) continue;
        if (strcmp(g_deparr_relations[i].elem1, procedure) == 0 && g_deparr_relations[i].elem2_type == TYPE_RUNWAY) {
            strncpy(runway[runway_count], g_deparr_relations[i].elem2, 19);
            runway[runway_count][19] = '\0';
            runway_count++;
        } else if (strcmp(g_deparr_relations[i].elem2, procedure) == 0 && g_deparr_relations[i].elem1_type == TYPE_RUNWAY) {
            strncpy(runway[runway_count], g_deparr_relations[i].elem1, 19);
            runway[runway_count][19] = '\0';
            runway_count++;
        }
    }
    return runway_count;
}

// 按机场+程序查询过渡点
int query_trans_by_proc(const char* airport, const char* procedure) {
    proc_trans_count = 0;
    for (int i = 0; i < g_deparr_relation_count && proc_trans_count < 100; i++) {
        if (strcmp(g_deparr_relations[i].airport, airport) != 0) continue;
        if (strcmp(g_deparr_relations[i].elem1, procedure) == 0 && g_deparr_relations[i].elem2_type == TYPE_WAYPOINT) {
            strncpy(proc_trans[proc_trans_count], g_deparr_relations[i].elem2, 19);
            proc_trans[proc_trans_count][19] = '\0';
            proc_trans_count++;
        } else if (strcmp(g_deparr_relations[i].elem2, procedure) == 0 && g_deparr_relations[i].elem1_type == TYPE_WAYPOINT) {
            strncpy(proc_trans[proc_trans_count], g_deparr_relations[i].elem1, 19);
            proc_trans[proc_trans_count][19] = '\0';
            proc_trans_count++;
        }
    }
    return proc_trans_count;
}
