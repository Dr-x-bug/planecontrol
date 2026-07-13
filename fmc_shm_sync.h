/**
 * fmc_shm_sync.h — FMC 数据同步回调机制
 *
 * ========== 设计原理 ==========
 * FMC模块需要向两个外部系统同步数据:
 *   1. ND导航显示 (通过共享内存)
 *   2. X-Plane FMC (通过UDP sendCOMM)
 *
 * 为避免FMC模块与外部系统耦合, 采用回调函数模式:
 *   - 主程序 (cockpit_main/cockpit_api) 注册回调实现
 *   - FMC模块 (fmc_buttons) 通过便捷宏调用回调
 *   - 回调内部完成实际的同步操作
 *
 * ========== 回调类型 ==========
 *   RouteSyncCallback        — 航路→共享内存同步 (无参数)
 *   XPlaneFmcSyncCallback    — 航路→X-Plane FMC同步 (传入origin/dest/flt_no)
 *
 * ========== 调用时机 ==========
 *   fmc_route_sync_call():  RTE页面修改航路后 / EXEC确认后 / DEP/ARR选择后
 *   fmc_xplane_sync_call(): RTE页面EXEC确认后
 */
#pragma once

/** 航路→共享内存同步回调 (无参数, FMC内部数据已就绪) */
typedef void (*RouteSyncCallback)(void);

/** 航路→X-Plane FMC同步回调 (传入起降机场和航班号) */
typedef void (*XPlaneFmcSyncCallback)(const char* origin, const char* dest, const char* flt_no);

// 全局回调函数指针 (主程序在初始化时设置, FMC模块只读取)
extern RouteSyncCallback g_route_sync_cb;
extern XPlaneFmcSyncCallback g_xplane_fmc_sync_cb;

/** 调用航路同步回调 (空指针安全) */
inline void fmc_route_sync_call() {
    if (g_route_sync_cb) {
        g_route_sync_cb();
    }
}

/** 调用X-Plane FMC同步回调 (空指针安全) */
inline void fmc_xplane_sync_call(const char* o, const char* d, const char* f) {
    if (g_xplane_fmc_sync_cb) {
        g_xplane_fmc_sync_cb(o, d, f);
    }
}
