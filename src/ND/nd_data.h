/**
 * nd_data.h — ND 模块数据层头文件
 * 职责: 声明导航数据结构、数据通信接口、全局变量
 * 依赖: SDL2, Windows API (多线程), X-Plane Connect
 */
#pragma once

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "xplaneConnect.h"
#ifdef __cplusplus
}
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  一、航路点数据结构
// ============================================================

struct Waypoint {
    std::string id;       // 航路点标识符
    double      lat;      // 纬度 (deg)
    double      lon;      // 经度 (deg)
};

// ============================================================
//  二、ND 渲染用导航数据
// ============================================================

struct NDData {
    double ac_lat;        // 纬度 (deg)
    double ac_lon;        // 经度 (deg)
    double heading;       // 航向 (0-360)
    double gs;            // 地速 (kt)
    double tas;           // 真空速 (kt)
    double wind_dir;      // 风向 (来向, deg)
    double wind_spd;      // 风速 (kt)
    int    raw_x;         // 原始 X 坐标 (nd.dat)
    int    raw_y;         // 原始 Y 坐标 (nd.dat)
};

extern NDData g_nd;

// ============================================================
//  三、X-Plane 飞行数据与多线程 (引用原定义, 过渡期)
//  完整定义位于 ../ND/nd_xpc.h 和 ../ND/nd_thread.h
//  后续将迁移至本模块
// ============================================================

#include "../ND/nd_xpc.h"     // NDFlightData, xpc_init/close, nd_fetch_all
#include "../ND/nd_thread.h"  // SharedNDBuffer, atomic_*, nd_thread_*

// 全局变量 (定义在 nd_data.c / nd_main.c 过渡期)
extern NDFlightData   g_nd_data;
extern SharedNDBuffer g_shared;
extern XPCSocket      g_xpc_sock;
extern bool           g_xpc_ready;

// ============================================================
//  六、导航点类型与哈希表 (网格化空间索引)
//  过渡期: 完整实现位于 ../ND/navaid_hash.h
//  后续将 GridHashTable 成员函数迁移至 nd_data.c
// ============================================================

// NavType / NavPoint / GridHashTable 定义由 navaid_hash.h 提供
#include "../ND/navaid_hash.h"

// ============================================================
//  七、数据层函数声明
// ============================================================

// --- 文件数据 ---
extern FILE* g_datafile;
extern long  g_total_lines;
extern long  g_current_line;

bool nd_data_open(const char* path);
bool nd_data_read_next();
void nd_data_close();

// --- 飞行计划 ---
std::vector<Waypoint> load_fms(const std::string& path);

// --- 导航台加载 (过渡期: 实现在 ../ND/navaid_hash.h) ---
// load_navaids / load_fixes / load_airports / query_nearby
// 上述函数当前以 inline 形式存在于 navaid_hash.h, 后续迁移至 nd_data.c

// --- X-Plane / 多线程 (过渡期: 实现在 ../ND/nd_xpc.h / nd_thread.h) ---
// xpc_init / xpc_close / nd_fetch_all / nd_thread_start / nd_thread_stop /
// atomic_swap_buffer / atomic_read_data
// 上述函数后续迁移至 nd_data.c
