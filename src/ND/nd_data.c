/**
 * nd_data.c — ND 模块数据层实现
 * 职责: 数据文件读取、X-Plane 通信、多线程同步、哈希表操作
 * 状态: 占位文件, 具体功能将在后续迭代中实现
 */
#include "nd_data.h"

// ============================================================
//  全局变量定义
// ============================================================

NDData         g_nd;
NDFlightData   g_nd_data;
SharedNDBuffer g_shared;
XPCSocket      g_xpc_sock;
bool           g_xpc_ready = false;

FILE* g_datafile     = nullptr;
long  g_total_lines  = 0;
long  g_current_line = 0;

// ============================================================
//  占位实现 (后续迭代中完善)
// ============================================================

// 以下函数的具体实现暂保留在原 ND/ 目录各 .h 文件中,
// 后续将逐步迁移至此 .c 文件, 实现声明与实现分离。

// 当前占位: GridHashTable 成员函数、nd_data_open/read_next/close、
// load_fms、load_navaids/fixes/airports、xpc_init/close、
// nd_fetch_all、nd_thread_start/stop 等。
