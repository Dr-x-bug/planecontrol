/**
 * fmc_data.c — FMC 模块数据层实现
 * 职责: 导航数据加载、AVL树构建、航路管理、DEP/ARR数据
 * 状态: 占位文件, 具体功能将在后续迭代中从 fmc_route.cpp / fmc_deparr.cpp 迁移
 */
#include "fmc_data.h"

// ============================================================
//  全局变量定义
// ============================================================

// 航路数据 (完整定义在 fmc_route.h)
RouteData g_route;

// DEP/ARR 状态
DepArrState g_deparr;

// 航路同步回调
RouteSyncCallback g_route_sync_cb = nullptr;

// ============================================================
//  占位实现 (后续迭代中完善)
// ============================================================

// 以下函数的具体实现暂保留在原 FMC/ 目录各 .cpp 文件中,
// 后续将逐步迁移至此 .c 文件:
//   - load_navdata_avl()    → 当前在 fmc_route.cpp
//   - load_route_from_fms() → 当前在 fmc_route.cpp
//   - fmc_sync_route_to_shm() → 当前在 main.cpp
