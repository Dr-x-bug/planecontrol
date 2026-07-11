/**
 * cockpit_main.cpp — X-Plane 综合座舱程序
 * 集成 ND (导航显示) + FMC (飞行管理计算机) 两个模块
 * 通过 XPlaneConnect 接入 X-Plane 实时飞行数据
 *
 * 编译: g++ -std=c++17 -I ND -I FMC -I . cockpit_main.cpp
 *        ND/xplaneConnect.c FMC/fmc_buttons.cpp FMC/fmc_buttons_init.cpp
 *        FMC/fmc_pages.cpp FMC/fmc_route.cpp FMC/fmc_deparr.cpp
 *        -o Cockpit.exe
 *        -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_gfx -lws2_32
 */

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <windows.h>

// ============================================================
// ND 模块 (Navigation Display)
// ============================================================
#include "ND/config.h"
#include "ND/renderer.h"
#include "ND/navdata.h"
#include "ND/nd_data.h"
#include "ND/nd_xpc.h"
#include "ND/nd_map.h"
#include "ND/navaid_hash.h"
#include "ND/nd_thread.h"

// ============================================================
// FMC 模块 (Flight Management Computer)
// ============================================================
#include "FMC/config.h"
#include "FMC/renderer.h"
#include "FMC/fmc_ui.h"
#include "FMC/fmc_pages.h"
#include "FMC/fmc_route.h"

// ============================================================
// 共享内存 IPC (FMC ↔ ND 数据联动)
// ============================================================
#include "shared_mem.h"
#include "fmc_shm_sync.h"

// 全局同步回调实例 (定义在fmc_shm_sync.h中声明)
RouteSyncCallback g_route_sync_cb = nullptr;

static SharedMemoryIPC g_shm_cockpit;
static LONG            g_shm_last_version = 0;

// 将FMC航路数据同步到共享内存
static void cockpit_sync_route_to_shm() {
    if (!g_shm_cockpit.pData) return;

    SharedRoutePoint shm_wpts[SHM_MAX_WPTS];
    int count = g_route.count;
    if (count > SHM_MAX_WPTS) count = SHM_MAX_WPTS;

    for (int i = 0; i < count; i++) {
        strncpy(shm_wpts[i].id, g_route.wpts[i].id, SHM_NAME_MAX - 1);
        shm_wpts[i].id[SHM_NAME_MAX - 1] = '\0';
        shm_wpts[i].lat  = g_route.wpts[i].lat;
        shm_wpts[i].lon  = g_route.wpts[i].lon;
        shm_wpts[i].freq = g_route.wpts[i].freq;
        shm_wpts[i].type = g_route.wpts[i].type;
    }

    g_shm_cockpit.write_route(shm_wpts, count,
        g_screen.origin, g_screen.dest, g_screen.flt_no);
}

// 从共享内存读取航路 (供ND使用, 同时支持外部ND进程连接)
static void cockpit_read_route_from_shm(std::vector<Waypoint>& out_wpts) {
    SharedRouteData route_data;
    if (g_shm_cockpit.pData && g_shm_cockpit.has_new_data(g_shm_last_version)) {
        if (g_shm_cockpit.read_route(route_data) && route_data.wpt_count > 0) {
            out_wpts.clear();
            for (int i = 0; i < route_data.wpt_count; i++) {
                Waypoint wp;
                wp.id  = route_data.wpts[i].id;
                wp.lat = route_data.wpts[i].lat;
                wp.lon = route_data.wpts[i].lon;
                out_wpts.push_back(wp);
            }
            printf("[Cockpit] ND synced %d waypoints from FMC (shm ver=%ld)\n",
                   route_data.wpt_count, route_data.version);
        }
    }
}

// ============================================================
// ND 模块全局变量定义
// (原本在 ND/main.cpp 中定义, 集成程序中在此统一定义)
// ============================================================
NDData         g_nd;
FILE*          g_datafile      = nullptr;
long           g_total_lines   = 0;
long           g_current_line  = 0;
NDFlightData   g_nd_data;
XPCSocket      g_xpc_sock;
bool           g_xpc_ready     = false;
SharedNDBuffer g_shared;

// ============================================================
// 全局状态
// ============================================================
static bool g_running          = true;
static bool g_use_xpc_thread   = false;
static HANDLE g_hXpcThread     = NULL;

// ND 模块全局变量 (定义于 ND 各头文件中)
// NDData g_nd, NDFlightData g_nd_data, XPCSocket g_xpc_sock, SharedNDBuffer g_shared 等

// ============================================================
// 辅助: FMC 显示 X-Plane 实时位置信息
// ============================================================
static void fmc_update_xplane_info() {
    if (!g_use_xpc_thread || !g_nd_data.pos_valid) return;

    // 更新 FMC 的 PROG 页面数据
    // (实际项目中会通过共享内存更新 FMC 屏幕数据)
    char buf[64];

    // 更新当前位置信息到 scratchpad 区域 (调试用)
    snprintf(buf, sizeof(buf), "POS %.4f %.4f", g_nd_data.lat, g_nd_data.lon);
    if (g_screen.current_page == PAGE_PROG) {
        g_screen.set_line_L(0, buf);
    }
}

// ============================================================
// ND 模块初始化
// ============================================================
static bool nd_module_init(NDRenderer& nd_renderer,
                           std::vector<Waypoint>& wpts,
                           GridHashTable& ht) {
    // 1. 初始化 SDL 渲染器 (ND 窗口)
    if (!nd_renderer.init()) {
        printf("[Cockpit] ND renderer init failed!\n");
        return false;
    }

    // 2. 加载飞行计划 (.fms)
    wpts = load_fms("ND/assets/KSEAKBFI.fms");
    printf("[Cockpit] ND loaded %zu waypoints\n", wpts.size());

    // 3. 启动 X-Plane 数据线程
    g_hXpcThread = nd_thread_start();
    if (g_hXpcThread) {
        Sleep(200);
        NDFlightData test;
        if (atomic_read_data(test) || g_shared.frame_count > 0) {
            g_use_xpc_thread = true;
            printf("[Cockpit] X-Plane data thread ACTIVE\n");
        } else {
            nd_thread_stop(g_hXpcThread);
            g_hXpcThread = NULL;
            printf("[Cockpit] No X-Plane connection, fallback to file\n");
        }
    }

    // 4. 回退: 文件数据模式
    if (!g_use_xpc_thread) {
        if (!nd_data_open("ND/assets/nd.dat")) {
            printf("[Cockpit] Failed to open nd.dat!\n");
            return false;
        }
        printf("[Cockpit] nd.dat opened: %ld lines (file mode)\n", g_total_lines);
    }

    // 5. 加载导航数据库 (哈希表)
    ht.init();
    load_navaids(ht, "ND/assets/earth_nav.dat");
    load_fixes(ht, "ND/assets/earth_fix.dat");
    load_airports(ht, "ND/assets/apt.dat");
    printf("[Cockpit] Hash table: %d nav points\n", ht.point_count);

    return true;
}

// ============================================================
// FMC 模块初始化
// ============================================================
static bool fmc_module_init(FMCRenderer& fmc_renderer) {
    if (!fmc_renderer.init()) {
        printf("[Cockpit] FMC renderer init failed!\n");
        return false;
    }

    // 加载导航数据到 AVL 树 + 飞行计划到航道数组
    load_navdata_avl("FMC/assets/earth_nav.dat",
                     "FMC/assets/earth_fix.dat",
                     "FMC/assets/apt.dat");
    load_route_from_fms("FMC/assets/KSEAKBFI.fms");
    printf("[Cockpit] FMC route: %d waypoints\n", g_route.count);

    // 初始化按钮并切换到初始页面
    fmc_buttons_init();
    fmc_switch_page(PAGE_INDEX);
    printf("[Cockpit] FMC initialized, page: %s\n",
           g_pages[g_screen.current_page].title);

    // 初始化共享内存 (FMC作为创建方)
    printf("[Cockpit] Initializing shared memory IPC...\n");
    if (g_shm_cockpit.create()) {
        cockpit_sync_route_to_shm();
    }

    // 注册航路同步回调 (供fmc_buttons调用)
    g_route_sync_cb = cockpit_sync_route_to_shm;

    return true;
}

// ============================================================
// ND 事件处理
// ============================================================
static void nd_handle_events(SDL_Window* nd_win) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        // 全局事件
        if (e.type == SDL_QUIT) {
            g_running = false;
            return;
        }

        // ND 窗口专属事件
        if (e.type == SDL_WINDOWEVENT &&
            e.window.windowID == SDL_GetWindowID(nd_win)) {
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                g_running = false;
                return;
            }
        }

        // ESC 退出
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            g_running = false;
            return;
        }
    }
}

// ============================================================
// FMC 事件处理
// ============================================================
static void fmc_handle_events(FMCRenderer& fmc_renderer) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        // 全局事件
        if (e.type == SDL_QUIT) {
            g_running = false;
            return;
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            g_running = false;
            return;
        }

        // 窗口大小调整
        if (e.type == SDL_WINDOWEVENT &&
            e.window.event == SDL_WINDOWEVENT_RESIZED) {
            int w, h;
            SDL_GetWindowSize(fmc_renderer.window, &w, &h);
            fmc_renderer.update_scale(w, h);
        }

        // 鼠标移动 → 按钮悬停检测
        if (e.type == SDL_MOUSEMOTION) {
            int mx = e.motion.x, my = e.motion.y, w, h;
            SDL_GetWindowSize(fmc_renderer.window, &w, &h);
            int idx = fmc_hit_test(mx, my, fmc_renderer.scale, w, h);
            static int hover_idx = -1;
            if (idx != hover_idx) {
                fmc_update_hover(idx);
                hover_idx = idx;
            }
        }

        // 鼠标点击 → 按钮按下
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            int mx = e.button.x, my = e.button.y, w, h;
            SDL_GetWindowSize(fmc_renderer.window, &w, &h);
            int idx = fmc_hit_test(mx, my, fmc_renderer.scale, w, h);
            if (idx >= 0) {
                printf("[FMC] Click: %s (page: %s)\n",
                       fmc_buttons[idx].label,
                       g_pages[g_screen.current_page].title);
                fmc_handle_click(idx);
            }
        }

        // 键盘输入 → 草稿栏
        if (e.type == SDL_TEXTINPUT && e.text.text[0] >= 32) {
            g_screen.type_char(e.text.text[0]);
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE) {
            g_screen.backspace();
        }
    }
}

// ============================================================
// ND 数据更新 (从 X-Plane 线程或文件读取)
// ============================================================
static void nd_update_data(Uint32& last_file_update) {
    if (g_use_xpc_thread) {
        // 多线程模式: 原子读取共享缓冲区
        NDFlightData latest;
        if (atomic_read_data(latest)) {
            g_nd.ac_lat   = latest.lat;
            g_nd.ac_lon   = latest.lon;
            g_nd.heading  = latest.heading;
            g_nd.gs       = latest.gs;
            g_nd.tas      = latest.tas;
            g_nd.wind_dir = latest.wind_dir;
            g_nd.wind_spd = latest.wind_spd;
        }
    } else {
        // 文件模式: 100ms 间隔读取
        Uint32 now = SDL_GetTicks();
        if (now - last_file_update >= 100) {
            last_file_update = now;
            nd_data_read_next();
            printf("\r[Cockpit] Frame:%ld/%ld  HDG=%.0f  GS=%.0fkt  ",
                   g_current_line, g_total_lines, g_nd.heading, g_nd.gs);
            fflush(stdout);
        }
    }
}

// ============================================================
// ND 渲染
// ============================================================
static void nd_render(NDRenderer& nd_renderer,
                      const std::vector<Waypoint>& wpts,
                      GridHashTable& ht) {
    nd_renderer.clear();
    draw_nd_map(nd_renderer, wpts, ht);

    // 状态栏
    char buf[64];
    if (g_use_xpc_thread) {
        snprintf(buf, sizeof(buf), "X-PLANE #%ld  GS:%.0fkt",
                 g_shared.frame_count, g_nd.gs);
        nd_renderer.draw_text(ND_WIN_W - 200, ND_WIN_H - 42, buf,
                              Color::GREEN_LT, true);
    } else {
        snprintf(buf, sizeof(buf), "FILE:%ld/%ld  GS:%.0fkt",
                 g_current_line, g_total_lines, g_nd.gs);
        nd_renderer.draw_text(ND_WIN_W - 200, ND_WIN_H - 42, buf,
                              Color::GRAY, true);
    }

    // X-Plane 连接状态
    if (g_use_xpc_thread) {
        const char* status = g_nd_data.connected ? "X-PLANE ONLINE" : "X-PLANE WAIT";
        SDL_Color c = g_nd_data.connected ? Color::GREEN_LT : Color::YELLOW;
        nd_renderer.draw_text(10, ND_WIN_H - 42, status, c, true);
    }

    nd_renderer.present();
}

// ============================================================
// FMC 渲染
// ============================================================
static void fmc_render(FMCRenderer& fmc_renderer) {
    fmc_renderer.clear();
    fmc_renderer.draw_bg();

    // 按键高亮
    for (int i = 0; i < FMC_KEY_COUNT; i++) {
        FMCButton& btn = fmc_buttons[i];
        if (!btn.label) continue;
        if (btn.shape == FMC_SHAPE_CIRCLE)
            fmc_renderer.draw_btn_circle(
                btn.rect.x + btn.rect.w / 2,
                btn.rect.y + btn.rect.h / 2,
                btn.radius, btn.state);
        else
            fmc_renderer.draw_btn_rect(
                btn.rect.x, btn.rect.y,
                btn.rect.w, btn.rect.h, btn.state);
    }

    // FMC 屏幕绘制
    fmc_draw_screen(fmc_renderer);

    // X-Plane 数据指示
    if (g_use_xpc_thread && g_nd_data.pos_valid) {
        char buf[64];
        snprintf(buf, sizeof(buf), "X-PLANE  HDG:%.0f  GS:%.0fkt  ALT:%.0fft",
                 g_nd_data.heading, g_nd_data.gs, g_nd_data.alt);
        fmc_renderer.draw_text(10, 960, buf, Color::FMC_CYAN, true);
    } else {
        char buf[48];
        snprintf(buf, sizeof(buf), "Page: %s  [NO X-PLANE]",
                 g_pages[g_screen.current_page].title);
        fmc_renderer.draw_text(10, 960, buf, Color::FMC_GREEN, true);
    }

    fmc_renderer.present();
}

// ============================================================
// 清理资源
// ============================================================
static void cockpit_cleanup(GridHashTable& ht) {
    // 停止 XPC 数据线程
    if (g_use_xpc_thread) {
        nd_thread_stop(g_hXpcThread);
    }
    nd_data_close();
    g_shm_cockpit.close();
    ht.destroy();
    // NDRenderer 和 FMCRenderer 的析构函数会自动清理 SDL 资源
    printf("\n[Cockpit] Shutdown complete.\n");
}

// ============================================================
// 主函数
// ============================================================
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    printf("========================================\n");
    printf("  Cockpit — ND + FMC + X-Plane Connect\n");
    printf("========================================\n\n");

    // ---- 全局 SDL 初始化 (只需一次) ----
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[Cockpit] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

    // ---- ND 模块 ----
    NDRenderer nd_renderer;
    std::vector<Waypoint> wpts;
    GridHashTable ht;

    if (!nd_module_init(nd_renderer, wpts, ht)) {
        printf("[Cockpit] ND init failed!\n");
        SDL_Quit();
        return 1;
    }

    // ---- FMC 模块 ----
    FMCRenderer fmc_renderer;
    if (!fmc_module_init(fmc_renderer)) {
        printf("[Cockpit] FMC init failed!\n");
        ht.destroy();
        SDL_Quit();
        return 1;
    }

    printf("\n[Cockpit] All modules initialized. Running...\n");
    printf("[Cockpit] ND window + FMC window — Press ESC or close any window to exit\n\n");

    // ---- 主循环 ----
    Uint32 last_file_update = 0;

    while (g_running) {
        // === 事件处理 (两个窗口) ===
        // 注意: SDL_PollEvent 会处理所有窗口的事件
        nd_handle_events(nd_renderer.window);
        if (!g_running) break;
        fmc_handle_events(fmc_renderer);
        if (!g_running) break;

        // === 数据更新 ===
        nd_update_data(last_file_update);

        // 同步 X-Plane 数据到 FMC
        fmc_update_xplane_info();

        // 定期检查共享内存: FMC→ND航路同步 (每100ms)
        static Uint32 last_shm_sync = 0;
        Uint32 now_shm = SDL_GetTicks();
        if (now_shm - last_shm_sync >= 100) {
            last_shm_sync = now_shm;
            cockpit_read_route_from_shm(wpts);
            // 同步飞机位置到共享内存
            if (g_use_xpc_thread && g_nd_data.pos_valid) {
                g_shm_cockpit.update_aircraft_position(
                    g_nd_data.lat, g_nd_data.lon, g_nd_data.heading);
            }
        }

        // === 渲染 ===
        nd_render(nd_renderer, wpts, ht);
        fmc_render(fmc_renderer);

        // VSync 约 60fps
        SDL_Delay(16);
    }

    // ---- 清理 ----
    cockpit_cleanup(ht);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
