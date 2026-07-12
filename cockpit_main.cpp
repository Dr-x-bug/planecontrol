/**
 * cockpit_main.cpp — Cockpit 入口程序
 *
 * 使用 CockpitContext API, 支持三种运行模式:
 *   1. 直接运行 (无参数):    阻塞式主循环
 *   2. --async (异步模式):   后台线程运行, 主线程监控
 *   3. --help:              显示帮助
 *
 * 编译: 参见 Makefile
 */

#include <winsock2.h>   // 必须在 windows.h 之前
#include <windows.h>
#include "cockpit_api.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// ============================================================
// 全局变量定义
// 注意: ND模块的全局变量在此定义 (原ND/main.cpp中的定义)
//       FMC模块的全局变量已在各自.cpp中定义, 此处仅通过extern引用
// ============================================================

// --- ND 模块全局变量 (仅在ND/main.cpp中有定义, 集成后移至此) ---
#include "ND/nd_data.h"
NDData         g_nd;
FILE*          g_datafile      = nullptr;
long           g_total_lines   = 0;
long           g_current_line  = 0;

#include "ND/nd_xpc.h"
NDFlightData   g_nd_data;
XPCSocket      g_xpc_sock;
bool           g_xpc_ready     = false;

#include "ND/nd_thread.h"
SharedNDBuffer g_shared;

// --- 同步回调 (主程序设置, FMC模块通过extern引用) ---
#include "fmc_shm_sync.h"
RouteSyncCallback        g_route_sync_cb      = nullptr;
XPlaneFmcSyncCallback    g_xplane_fmc_sync_cb = nullptr;

// ============================================================
// 帮助信息
// ============================================================
static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]\n\n", prog);
    printf("  (no args)    阻塞式运行 Cockpit (主线程运行主循环)\n");
    printf("  --async      异步模式: 后台线程运行, 主线程监控状态\n");
    printf("  --help       显示此帮助信息\n\n");
    printf("CockpitContext API 也可被其他程序链接调用:\n");
    printf("  #include \"cockpit_api.h\"\n");
    printf("  CockpitContext ctx;\n");
    printf("  ctx.init();\n");
    printf("  ctx.start();   // 异步启动\n");
    printf("  // ... 你的代码 ...\n");
    printf("  ctx.stop();    // 等待退出\n");
    printf("  ctx.cleanup();\n");
}

// ============================================================
// 阻塞式运行
// ============================================================
static int run_blocking() {
    CockpitContext ctx;

    if (!ctx.init()) {
        fprintf(stderr, "[Cockpit] Init failed!\n");
        return 1;
    }

    ctx.run();       // 阻塞直到用户关闭窗口
    ctx.cleanup();
    return 0;
}

// ============================================================
// 异步运行 (演示多线程调用)
// ============================================================
static int run_async() {
    CockpitContext ctx;

    if (!ctx.init()) {
        fprintf(stderr, "[Cockpit] Init failed!\n");
        return 1;
    }

    printf("[Main] Starting cockpit in background thread...\n");
    if (!ctx.start()) {
        fprintf(stderr, "[Main] Failed to start cockpit thread!\n");
        ctx.cleanup();
        return 1;
    }

    printf("[Main] Cockpit running in background.\n");
    printf("[Main] Press ENTER in this console to stop...\n\n");

    // 主线程: 定期查询状态 (演示线程安全的状态查询)
    while (ctx.is_running()) {
        CockpitFlightState state = ctx.get_flight_state();

        if (state.pos_valid) {
            printf("\r[Main] Frame:%d  LAT:%.4f  LON:%.4f  HDG:%.0f  GS:%.0fkt  XPlane:%s  ",
                   state.frame_count, state.lat, state.lon,
                   state.heading, state.gs,
                   state.xplane_connected ? "ON" : "OFF");
            fflush(stdout);
        }

        // 检查用户是否按了回车
        if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
            printf("\n[Main] ENTER pressed, stopping...\n");
            break;
        }

        Sleep(200);
    }

    printf("\n[Main] Stopping cockpit...\n");
    ctx.stop();
    ctx.cleanup();

    printf("[Main] Cockpit exited. Goodbye!\n");
    return 0;
}

// ============================================================
// 主入口
// ============================================================
int main(int argc, char* argv[]) {
    // 解析命令行参数
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(argv[1], "--async") == 0) {
            return run_async();
        }
        printf("Unknown option: %s\n", argv[1]);
        print_help(argv[0]);
        return 1;
    }

    // 默认: 阻塞式运行
    return run_blocking();
}
