/**
 * cockpit_main.cpp — Cockpit 入口程序
 *
 * 编译: mingw32-make
 * 运行: ./Cockpit.exe
 */

#include "cockpit.h"

// ============================================================
// 全局变量定义
// ============================================================
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

#include "fmc_shm_sync.h"
RouteSyncCallback        g_route_sync_cb      = nullptr;
XPlaneFmcSyncCallback    g_xplane_fmc_sync_cb = nullptr;

// ============================================================
// 主入口
// ============================================================
int main() {
    CockpitContext ctx;

    if (!ctx.init()) {
        fprintf(stderr, "[Cockpit] Init failed!\n");
        return 1;
    }

    ctx.run();       // 阻塞直到用户关闭窗口
    ctx.cleanup();

    printf("[Cockpit] Goodbye!\n");
    return 0;
}
