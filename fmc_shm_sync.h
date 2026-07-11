/**
 * fmc_shm_sync.h — FMC→ND 共享内存同步回调 + X-Plane sendCOMM 同步
 */
#pragma once

// 航路同步回调类型
typedef void (*RouteSyncCallback)(void);
// X-Plane FMC 同步回调 (origin/dest/flt_no → sendCOMM)
typedef void (*XPlaneFmcSyncCallback)(const char* origin, const char* dest, const char* flt_no);

// 全局同步回调 (由main程序设置)
extern RouteSyncCallback g_route_sync_cb;
extern XPlaneFmcSyncCallback g_xplane_fmc_sync_cb;

// 便捷宏: 调用同步回调
inline void fmc_route_sync_call() {
    if (g_route_sync_cb) {
        g_route_sync_cb();
    }
}
inline void fmc_xplane_sync_call(const char* o, const char* d, const char* f) {
    if (g_xplane_fmc_sync_cb) {
        g_xplane_fmc_sync_cb(o, d, f);
    }
}
