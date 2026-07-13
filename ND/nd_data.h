/**
 * nd_data.h — ND 文件模式数据读取 (离线回放)
 *
 * ========== 用途 ==========
 * 当 X-Plane 不可用时, 从本地 CSV 文件 nd.dat 读取模拟飞行数据,
 * 实现离线开发和演示。数据文件循环读取 (fseek 回文件头)。
 *
 * ========== 数据格式 ==========
 * nd.dat 每行: x_pos, y_pos, heading, speed (CSV, 无表头, 约2000行)
 *   例: 320,450,270,30
 *
 * ========== NDData 字段 ==========
 *   ac_lat/ac_lon — 飞机经纬度 (文件模式下未使用, 由坐标推算)
 *   heading        — 航向 (0-360°)
 *   gs             — 地速 (kt, speed×10)
 *   tas            — 真空速 (kt, gs-10 估算)
 *   wind_dir       — 风向 (模拟值, hdg+120°)
 *   wind_spd       — 风速 (模拟值, spd%20+15)
 */

#pragma once
#include <cstdio>
#include <cstring>

// ===== 全局ND导航数据变量 =====
// 数据文件格式: x_pos, y_pos, heading, speed (CSV, 无表头)
// 共2000行，每行代表一帧的导航数据
// 读取完毕后 fseek 回文件头循环

struct NDData {
    double ac_lat;        // 纬度 (度)
    double ac_lon;        // 经度 (度)
    double heading;       // 航向 (0-360)
    double gs;            // 地速 (kt)
    double tas;           // 真空速 (kt)
    double wind_dir;      // 风向 (来向, 度)
    double wind_spd;      // 风速 (kt)

    // 从 nd.dat 原始字段转换而来
    int raw_x;            // 原始 X 坐标
    int raw_y;            // 原始 Y 坐标
};

// 全局数据实例
extern NDData g_nd;
// 文件指针（全局，支持循环读取）
extern FILE* g_datafile;
// 文件总行数
extern long g_total_lines;
// 当前行号
extern long g_current_line;

// 打开数据文件
inline bool nd_data_open(const char* path) {
    g_datafile = fopen(path, "r");
    if (!g_datafile) return false;

    // 统计总行数
    g_total_lines = 0;
    char buf[128];
    while (fgets(buf, sizeof(buf), g_datafile)) {
        g_total_lines++;
    }
    // 重置文件指针到开头
    fseek(g_datafile, 0, SEEK_SET);
    g_current_line = 0;
    return true;
}

// 读取一行数据（fgets + sscanf 格式化解析）
inline bool nd_data_read_next() {
    if (!g_datafile) return false;

    char line[128];
    // fgets: 读取一行
    if (!fgets(line, sizeof(line), g_datafile)) {
        // 文件末尾: fseek 重置到开头，循环读取
        fseek(g_datafile, 0, SEEK_SET);
        g_current_line = 0;
        if (!fgets(line, sizeof(line), g_datafile)) {
            return false; // 空文件
        }
    }

    // sscanf: 格式化解析 CSV 行
    // 格式: x_pos, y_pos, heading, speed
    int x, y, hdg, spd;
    int n = sscanf(line, "%d,%d,%d,%d", &x, &y, &hdg, &spd);
    if (n < 4) return false;

    // 存入全局变量
    g_nd.raw_x   = x;
    g_nd.raw_y   = y;
    g_nd.heading = (double)hdg;
    g_nd.gs      = (double)spd * 10.0;
    g_nd.tas     = g_nd.gs - 10.0;

    // 模拟风数据（基于速度和航向变化）
    g_nd.wind_dir = fmod(hdg + 120.0, 360.0);
    g_nd.wind_spd = (double)(spd % 20) + 15.0;

    // 基于 XY 坐标模拟经纬度变化（KSEA附近）
    g_nd.ac_lat = 47.45 + (y - 260) * 0.005;
    g_nd.ac_lon = -122.31 + (x - 260) * 0.008;

    g_current_line++;
    return true;
}

// 关闭数据文件
inline void nd_data_close() {
    if (g_datafile) {
        fclose(g_datafile);
        g_datafile = nullptr;
    }
}
