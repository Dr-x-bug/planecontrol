#pragma once
#include <cstring>

// ===== 跑道 =====
struct Runway {
    char id[8];          // "16L", "34R" 等
    double lat, lon;
    double heading;      // 跑道航向
    int    length_ft;    // 跑道长度
};

// ===== 离场/进场程序 =====
struct Procedure {
    char   name[16];      // "SEA5", "GLASR1"
    char   type;          // 'S'=SID, 'T'=STAR, 'A'=APPROACH
    char   runway[8];     // 关联跑道 (空=任意)
    char   transitions[5][16]; // 过渡点列表
    int    trans_count;
};

// ===== DEP/ARR 选择状态 =====
struct DepArrState {
    // 离场 (DEP KSEA)
    char   dep_rwy[8];        // 选中跑道
    char   dep_sid[16];       // 选中SID
    char   dep_trans[16];     // 选中过渡点

    // 进场 (ARR KBFI)
    char   arr_rwy[8];
    char   arr_star[16];
    char   arr_trans[16];
    char   arr_appr[16];      // 进近方式

    // 选择阶段
    int    dep_step;  // 0=选跑道, 1=选SID, 2=选过渡点
    int    arr_step;

    DepArrState() { clear(); }
    void clear() {
        memset(this, 0, sizeof(*this));
        dep_step = 0; arr_step = 0;
    }
};

// ===== 预定义跑道数据 (KSEA / KBFI) =====
extern Runway ksea_runways[];
extern int    ksea_rwy_count;
extern Runway kbfi_runways[];
extern int    kbfi_rwy_count;

// ===== 预定义离场程序 (KSEA SIDs) =====
extern Procedure ksea_sids[];
extern int       ksea_sid_count;

// ===== 预定义进场程序 (KBFI STARs) =====
extern Procedure kbfi_stars[];
extern int       kbfi_star_count;

// ===== 全局状态 =====
extern DepArrState g_deparr;
