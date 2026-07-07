#include <SDL2/SDL.h>
#include <cstdio>
#include "config.h"
#include "renderer.h"
#include "navdata.h"
#include "nd_data.h"
#include "nd_xpc.h"
#include "nd_map.h"
#include "navaid_hash.h"
#include "nd_thread.h"

// ===== 全局变量定义 =====
NDData g_nd;
FILE*  g_datafile    = nullptr;
long   g_total_lines = 0;
long   g_current_line = 0;

NDFlightData    g_nd_data;
XPCSocket       g_xpc_sock;
bool            g_xpc_ready = false;
SharedNDBuffer  g_shared;      // 多线程共享缓冲区

// 数据同步到渲染层
void sync_to_nd(const NDFlightData& src) {
    g_nd.ac_lat   = src.lat;
    g_nd.ac_lon   = src.lon;
    g_nd.heading  = src.heading;
    g_nd.gs       = src.gs;
    g_nd.tas      = src.tas;
    g_nd.wind_dir = src.wind_dir;
    g_nd.wind_spd = src.wind_spd;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    Renderer renderer;
    if (!renderer.init()) {
        printf("Failed to init SDL2!\n");
        return 1;
    }

    auto wpts = load_fms("assets/KSEAKBFI.fms");
    printf("ND MAP Mode - Loaded %zu waypoints\n", wpts.size());

    // === 数据源选择: 优先多线程XPC, 回退文件 ===
    HANDLE hThread = NULL;
    bool   use_thread = false;

    hThread = nd_thread_start();
    if (hThread) {
        // 等待首帧数据
        Sleep(200);
        NDFlightData test;
        if (atomic_read_data(test) || g_shared.frame_count > 0) {
            use_thread = true;
            sync_to_nd(g_shared.buf[g_shared.read_idx]);
            printf("[ND] Multi-threaded XPC mode active (thread)\n");
        } else {
            // 数据线程无法获取数据，回退
            nd_thread_stop(hThread);
            hThread = NULL;
            printf("[ND] Thread no X-Plane data, fallback to file\n");
        }
    }

    if (!use_thread) {
        if (!nd_data_open("assets/nd.dat")) {
            printf("Failed to open nd.dat!\n");
            return 1;
        }
        printf("nd.dat opened: %ld lines (single-thread file mode)\n", g_total_lines);
    }

    // === 哈希表 ===
    GridHashTable ht;
    ht.init();
    int nav_count = load_navaids(ht, "assets/earth_nav.dat");
    int fix_count = load_fixes(ht, "assets/earth_fix.dat");
    int apt_count = load_airports(ht, "assets/apt.dat");
    printf("[Hash] Total: %d points\n", ht.point_count);
    (void)nav_count; (void)fix_count; (void)apt_count;

    // === 主循环 (渲染线程) ===
    bool running = true;
    Uint32 last_file_update = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;

            // 窗口缩放事件
            if (e.type == SDL_WINDOWEVENT &&
                e.window.event == SDL_WINDOWEVENT_RESIZED) {
                // SDL 自动处理缩放，无需额外操作
            }
        }

        // === 数据更新 ===
        if (use_thread) {
            // 多线程模式: 原子读取共享缓冲区
            NDFlightData latest;
            if (atomic_read_data(latest)) {
                sync_to_nd(latest);
            }
        } else {
            // 文件模式: 直接读取 (100ms间隔)
            Uint32 now = SDL_GetTicks();
            if (now - last_file_update >= 100) {
                last_file_update = now;
                nd_data_read_next();
                printf("\rFrame: %ld/%ld  HDG=%.0f  GS=%.0fkt",
                       g_current_line, g_total_lines, g_nd.heading, g_nd.gs);
                fflush(stdout);
            }
        }

        // === 渲染 ===
        renderer.clear();
        draw_nd_map(renderer, wpts, ht);

        // 状态栏
        char buf[64];
        if (use_thread) {
            snprintf(buf, sizeof(buf), "THREAD #%ld",
                     g_shared.frame_count);
            renderer.draw_text(WIN_W - 120, WIN_H - 42, buf,
                Color::GREEN_LT, true);
        } else {
            snprintf(buf, sizeof(buf), "FILE:%ld/%ld",
                     g_current_line, g_total_lines);
            renderer.draw_text(WIN_W - 120, WIN_H - 42, buf,
                Color::GRAY, true);
        }
        renderer.present();
        SDL_Delay(16);
    }

    // === 资源清理 ===
    if (use_thread) {
        nd_thread_stop(hThread);
    }
    nd_data_close();
    ht.destroy();
    printf("\nND stopped.\n");
    return 0;
}
