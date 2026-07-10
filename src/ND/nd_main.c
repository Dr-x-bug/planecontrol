/**
 * nd_main.c — ND 模块程序入口
 * 职责: 模块初始化、主循环、事件分发、资源释放
 *
 * 架构说明:
 *   nd_main.c (入口/流程控制)
 *     ├── nd_ui.h / nd_ui.c     (界面渲染)
 *     └── nd_data.h / nd_data.c  (数据通信)
 *
 * 过渡期: 部分数据层函数仍位于原 ND/ 目录各 .h 文件中,
 *         后续将逐步迁移至 nd_data.c
 */
#include "nd_ui.h"
#include "nd_data.h"

// 过渡期引用: 共享内存 IPC
#include "../shared_mem.h"       // SharedMemoryIPC

#include <cstdio>
#include <cstring>

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

// 共享内存 IPC (从 FMC 读取航路数据)
static SharedMemoryIPC g_shm_nd;
static LONG            g_shm_last_version = 0;

// ============================================================
//  辅助函数
// ============================================================

/** 将多线程共享缓冲区的飞行数据同步到渲染层 */
static void sync_to_nd(const NDFlightData& src) {
    g_nd.ac_lat   = src.lat;
    g_nd.ac_lon   = src.lon;
    g_nd.heading  = src.heading;
    g_nd.gs       = src.gs;
    g_nd.tas      = src.tas;
    g_nd.wind_dir = src.wind_dir;
    g_nd.wind_spd = src.wind_spd;
}

/** 从共享内存读取 FMC 航路数据并转换为 Waypoint 列表 */
static void nd_read_route_from_shm(std::vector<Waypoint>& out_wpts) {
    SharedRouteData route_data;
    if (g_shm_nd.pData && g_shm_nd.has_new_data(g_shm_last_version)) {
        if (g_shm_nd.read_route(route_data) && route_data.wpt_count > 0) {
            out_wpts.clear();
            for (int i = 0; i < route_data.wpt_count; i++) {
                Waypoint wp;
                wp.id  = route_data.wpts[i].id;
                wp.lat = route_data.wpts[i].lat;
                wp.lon = route_data.wpts[i].lon;
                out_wpts.push_back(wp);
            }
            printf("[ND] Loaded %d waypoints from FMC via shared memory (ver=%ld)\n",
                   route_data.wpt_count, route_data.version);
        }
    }
}

// ============================================================
//  主函数
// ============================================================

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // ---- 1. 界面初始化 ----
    NDRenderer renderer;
    if (!renderer.init()) {
        printf("[ND] Failed to init SDL2!\n");
        return 1;
    }

    // ---- 2. 加载飞行计划 ----
    auto wpts = load_fms("assets/KSEAKBFI.fms");
    printf("ND MAP Mode - Loaded %zu waypoints\n", wpts.size());

    // ---- 3. 共享内存 IPC 初始化 ----
    printf("[ND] Opening shared memory IPC...\n");
    if (g_shm_nd.open()) {
        g_shm_last_version = 0;
        nd_read_route_from_shm(wpts);
    } else {
        printf("[ND] Shared memory not available, using .fms file data\n");
    }

    // ---- 4. 数据源选择: 优先多线程 X-Plane, 回退文件 ----
    HANDLE hThread   = NULL;
    bool   use_thread = false;

    hThread = nd_thread_start();
    if (hThread) {
        Sleep(200);
        NDFlightData test;
        if (atomic_read_data(test) || g_shared.frame_count > 0) {
            use_thread = true;
            sync_to_nd(g_shared.buf[g_shared.read_idx]);
            printf("[ND] Multi-threaded XPC mode active (thread)\n");
        } else {
            nd_thread_stop(hThread);
            hThread = NULL;
            printf("[ND] Thread no X-Plane data, fallback to file\n");
        }
    }

    if (!use_thread) {
        if (!nd_data_open("assets/nd.dat")) {
            printf("[ND] Failed to open nd.dat!\n");
            return 1;
        }
        printf("nd.dat opened: %ld lines (single-thread file mode)\n", g_total_lines);
    }

    // ---- 5. 哈希表初始化 (导航台空间索引) ----
    GridHashTable ht;
    ht.init();
    int nav_count = load_navaids(ht, "assets/earth_nav.dat");
    int fix_count = load_fixes(ht, "assets/earth_fix.dat");
    int apt_count = load_airports(ht, "assets/apt.dat");
    printf("[Hash] Total: %d points\n", ht.point_count);
    (void)nav_count; (void)fix_count; (void)apt_count;

    // ---- 6. 主循环 ----
    bool   running = true;
    Uint32 last_file_update = 0;

    while (running) {
        // --- 事件处理 ---
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        // --- 数据更新 ---
        if (use_thread) {
            NDFlightData latest;
            if (atomic_read_data(latest)) {
                sync_to_nd(latest);
            }
        } else {
            Uint32 now = SDL_GetTicks();
            if (now - last_file_update >= 100) {
                last_file_update = now;
                nd_data_read_next();
                printf("\rFrame: %ld/%ld  HDG=%.0f  GS=%.0fkt",
                       g_current_line, g_total_lines, g_nd.heading, g_nd.gs);
                fflush(stdout);
            }
        }

        // 定期检查共享内存航路更新
        static Uint32 last_shm_check = 0;
        if (g_shm_nd.pData) {
            Uint32 now_shm = SDL_GetTicks();
            if (now_shm - last_shm_check >= 100) {
                last_shm_check = now_shm;
                nd_read_route_from_shm(wpts);
            }
        }

        // --- 渲染 ---
        renderer.clear();
        draw_nd_map(renderer, wpts, ht);

        // 状态栏提示
        char buf[64];
        if (use_thread) {
            snprintf(buf, sizeof(buf), "THREAD #%ld", g_shared.frame_count);
            renderer.draw_text(ND_WIN_W - 120, ND_WIN_H - 42, buf, Color::GREEN_LT, true);
        } else {
            snprintf(buf, sizeof(buf), "FILE:%ld/%ld", g_current_line, g_total_lines);
            renderer.draw_text(ND_WIN_W - 120, ND_WIN_H - 42, buf, Color::GRAY, true);
        }

        renderer.present();
        SDL_Delay(16);
    }

    // ---- 7. 资源清理 ----
    if (use_thread) {
        nd_thread_stop(hThread);
    }
    nd_data_close();
    g_shm_nd.close();
    ht.destroy();
    printf("\nND stopped.\n");
    TTF_Quit();
    SDL_Quit();
    return 0;
}
