#pragma once
#include <cstdio>
#include <cstring>
#include <cmath>

#ifdef __cplusplus
extern "C" {
#endif
#include "xplaneConnect.h"
#ifdef __cplusplus
}
#endif

// ===== ND 数据结构定义 =====
// 包含有效性标志，用于容错处理

struct NDFlightData {
    // === 位置数据 (getPOSI) ===
    double lat;           // 纬度 (deg)
    double lon;           // 经度 (deg)
    double alt;           // 气压高度 (ft MSL)
    bool   pos_valid;     // 位置数据有效标志

    // === 姿态数据 (getPOSI) ===
    double pitch;         // 俯仰角 (deg)
    double roll;          // 横滚角 (deg)
    double heading;       // 真航向 (deg)
    bool   att_valid;     // 姿态数据有效标志

    // === 速度数据 (getDREFs) ===
    double gs;            // 地速 (kt)
    double tas;           // 真空速 (kt)
    double vs;            // 垂直速度 (ft/min)
    bool   spd_valid;     // 速度数据有效标志

    // === 风数据 (getDREFs) ===
    double wind_dir;      // 风向 (来向, deg)
    double wind_spd;      // 风速 (kt)
    bool   wind_valid;    // 风数据有效标志

    // === 连接状态 ===
    bool   connected;     // X-Plane 连接状态
    int    error_count;   // 连续错误计数
};

// 全局ND飞行数据实例
extern NDFlightData g_nd_data;

// ===== X-Plane Connect 接口函数 =====

// XPC 连接对象
extern XPCSocket g_xpc_sock;
extern bool      g_xpc_ready;

// 初始化 X-Plane 连接
inline bool xpc_init(const char* ip = "127.0.0.1", unsigned short port = 49009) {
    g_xpc_sock = openUDP(ip);
    // 设置 X-Plane 端口
    setCONN(&g_xpc_sock, port);
    g_xpc_ready = true;
    g_nd_data.connected = false;
    g_nd_data.error_count = 0;

    // 初始化有效性标志
    g_nd_data.pos_valid  = false;
    g_nd_data.att_valid  = false;
    g_nd_data.spd_valid  = false;
    g_nd_data.wind_valid = false;

    printf("[XPC] Connected to %s:%d\n", ip, port);
    return true;
}

// 关闭 X-Plane 连接
inline void xpc_close() {
    if (g_xpc_ready) {
        closeUDP(g_xpc_sock);
        g_xpc_ready = false;
        g_nd_data.connected = false;
        printf("[XPC] Disconnected\n");
    }
}

// ===== 批量获取速度与风数据 (getDREFs) =====
// 知识点: getDREFs接口的批量数据获取
inline bool fetch_speed_wind() {
    if (!g_xpc_ready) return false;

    // 定义要获取的 dataref 名称数组
    const char* drefs[] = {
        "sim/flightmodel/position/groundspeed",      // 地速 m/s
        "sim/flightmodel/position/true_airspeed",    // 真空速 m/s
        "sim/flightmodel/position/vh_ind_fpm",       // 垂直速度 ft/min
        "sim/weather/wind_direction_degt",           // 风向 deg
        "sim/weather/wind_speed_kt"                  // 风速 kt
    };
    const unsigned char count = 5;

    // 为每个 dataref 分配值数组
    float gs_buf[1]    = {0};
    float tas_buf[1]   = {0};
    float vs_buf[1]    = {0};
    float wdir_buf[1]  = {0};
    float wspd_buf[1]  = {0};

    float* values[] = {gs_buf, tas_buf, vs_buf, wdir_buf, wspd_buf};
    int sizes[] = {1, 1, 1, 1, 1};

    // 调用 getDREFs 批量获取
    int result = getDREFs(g_xpc_sock, drefs, values, count, sizes);

    // 容错处理: 检查返回值
    if (result < 0) {
        g_nd_data.spd_valid  = false;
        g_nd_data.wind_valid = false;
        g_nd_data.error_count++;
        return false;
    }

    // 数据有效性判断 (检测异常值)
    float gs_ms   = gs_buf[0];
    float tas_ms  = tas_buf[0];
    float vs_fpm  = vs_buf[0];
    float wdir    = wdir_buf[0];
    float wspd    = wspd_buf[0];

    // 异常值检查: 速度应在合理范围
    if (gs_ms > 0.1f && gs_ms < 500.0f) {
        g_nd_data.gs  = gs_ms * 1.94384;  // m/s → kt
        g_nd_data.tas = tas_ms * 1.94384;
        g_nd_data.vs  = vs_fpm;
        g_nd_data.spd_valid = true;
    } else {
        g_nd_data.spd_valid = false;
    }

    // 风数据有效性
    if (wdir >= 0.0f && wdir <= 360.0f && wspd >= 0.0f && wspd < 200.0f) {
        g_nd_data.wind_dir = wdir;
        g_nd_data.wind_spd = wspd;
        g_nd_data.wind_valid = true;
    } else {
        g_nd_data.wind_valid = false;
    }

    g_nd_data.connected = true;
    g_nd_data.error_count = 0;
    return true;
}

// ===== 获取位置与姿态 (getPOSI) =====
// 知识点: 接口调用失败的异常处理
inline bool fetch_position() {
    if (!g_xpc_ready) return false;

    double posi[7] = {0};  // [Lat, Lon, Alt, Pitch, Roll, Yaw, Gear]

    int result = getPOSI(g_xpc_sock, posi, 0);  // ac=0 本机

    // 异常处理: 检查返回值
    if (result < 0) {
        g_nd_data.pos_valid = false;
        g_nd_data.att_valid = false;
        g_nd_data.error_count++;
        g_nd_data.connected = false;
        return false;
    }

    // 数据有效性判断
    double lat  = posi[0];
    double lon  = posi[1];
    double alt  = posi[2];
    double pitch = posi[3];
    double roll  = posi[4];
    double yaw   = posi[5];

    // 经纬度范围检查
    if (lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0) {
        g_nd_data.lat  = lat;
        g_nd_data.lon  = lon;
        g_nd_data.alt  = alt * 3.28084;  // m → ft
        g_nd_data.pos_valid = true;
    } else {
        g_nd_data.pos_valid = false;
    }

    // 姿态范围检查
    if (pitch >= -90.0 && pitch <= 90.0 && roll >= -180.0 && roll <= 180.0) {
        g_nd_data.pitch   = pitch;
        g_nd_data.roll    = roll;
        g_nd_data.heading = yaw;  // yaw = true heading from getPOSI
        g_nd_data.att_valid = true;
    } else {
        g_nd_data.att_valid = false;
    }

    g_nd_data.connected = true;
    g_nd_data.error_count = 0;
    return true;
}

// ===== 统一数据更新 =====
// 尝试从 X-Plane 获取数据，失败则返回 false
inline bool nd_fetch_all() {
    bool ok = true;

    // 获取位置和姿态
    if (!fetch_position()) {
        ok = false;
    }

    // 批量获取速度和风数据
    if (!fetch_speed_wind()) {
        ok = false;
    }

    return ok;
}
