/**
 * fmc_shm_sync.h — FMC→ND 共享内存同步回调
 *
 * 使用函数指针实现: FMC standalone和Cockpit集成版
 * 可以设置不同的共享内存实例进行航路同步
 */
#pragma once

// 航路同步回调类型
typedef void (*RouteSyncCallback)(void);

// 全局同步回调 (由main程序设置)
extern RouteSyncCallback g_route_sync_cb;

// 便捷宏: 调用同步回调
inline void fmc_route_sync_call() {
    if (g_route_sync_cb) {
        g_route_sync_cb();
    }
}
