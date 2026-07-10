/**
 * fmc_data.h — FMC 模块数据层头文件
 * 职责: 声明航路数据结构、AVL树、跑道/程序数据、全局状态
 * 过渡期: 完整实现引用原 ../FMC/fmc_route.h 和 ../FMC/fmc_deparr.h
 */
#pragma once

// 过渡期: 引用原模块实现, 后续迁移至 fmc_data.c
#include "../FMC/fmc_route.h"
#include "../FMC/fmc_deparr.h"

// ============================================================
//  全局变量声明 (定义在 fmc_data.c / fmc_main.c 过渡期)
// ============================================================

#include <cstdio>

// 航路数据 (fmc_route.h)
extern RouteData g_route;

// DEP/ARR 选择状态 (fmc_deparr.h)
extern DepArrState g_deparr;

// ============================================================
//  数据层函数声明
// ============================================================

// 加载导航数据到AVL树
void load_navdata_avl(const char* nav_path, const char* fix_path, const char* apt_path);

// 从 .fms 文件加载飞行计划到航路数组
void load_route_from_fms(const char* fms_path);

// 保存/同步航路
void fmc_sync_route_to_shm();

// 航路同步回调函数指针类型
typedef void (*RouteSyncCallback)(void);
extern RouteSyncCallback g_route_sync_cb;
inline void fmc_route_sync_call() {
    if (g_route_sync_cb) g_route_sync_cb();
}
