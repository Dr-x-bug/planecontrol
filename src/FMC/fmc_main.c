/**
 * fmc_main.c — FMC 模块程序入口
 * 职责: 模块初始化、主循环、事件分发、资源释放
 *
 * 架构说明:
 *   fmc_main.c (入口/流程控制)
 *     ├── fmc_ui.h / fmc_ui.c       (界面渲染)
 *     └── fmc_data.h / fmc_data.c   (数据通信)
 *
 * 过渡期: 部分函数仍位于原 FMC/ 目录各文件中,
 *         后续将逐步迁移至本模块
 */
#include "fmc_ui.h"
#include "fmc_data.h"

// 过渡期引用: 原模块实现尚未完全迁移
//   - fmc_buttons_init / fmc_handle_click → FMC/fmc_buttons_init.cpp + fmc_buttons.cpp
//   - fmc_switch_page / fmc_draw_screen    → FMC/fmc_pages.h
//   - 共享内存 IPC                         → shared_mem.h
//   - 航路同步回调                          → fmc_shm_sync.h
//
// 当前通过 FMC/Makefile 编译原 main.cpp,
// 本文件为后续迁移的目标入口文件

#include <cstdio>
#include <cstring>

// ============================================================
//  全局变量
// ============================================================

SharedMemoryIPC g_shm;

// ============================================================
//  辅助函数
// ============================================================

/** 将 g_route 中的航路数据写入共享内存 */
static void fmc_sync_route_to_shm() {
    if (!g_shm.pData) return;

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

    g_shm.write_route(shm_wpts, count,
        g_screen.origin, g_screen.dest, g_screen.flt_no);
    printf("[FMC] Synced %d waypoints to shared memory\n", count);
}

// ============================================================
//  主函数
// ============================================================

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // ---- 1. 界面初始化 ----
    FMCRenderer renderer;
    if (!renderer.init()) {
        printf("[FMC] Renderer init failed!\n");
        return 1;
    }

    // ---- 2. 加载导航数据 ----
    printf("[FMC] Loading nav data...\n");
    load_navdata_avl("assets/earth_nav.dat", "assets/earth_fix.dat", "assets/apt.dat");
    load_route_from_fms("assets/KSEAKBFI.fms");
    printf("[FMC] Route: %d waypoints, %d pages\n",
           g_route.count, g_route.total_pages());

    // ---- 3. 共享内存 IPC ----
    printf("[FMC] Initializing shared memory IPC...\n");
    if (g_shm.create()) {
        fmc_sync_route_to_shm();
    }

    // ---- 4. 注册回调 ----
    g_route_sync_cb = fmc_sync_route_to_shm;

    // ---- 5. 按钮初始化与默认页面 ----
    fmc_buttons_init();
    fmc_switch_page(PAGE_INIT_REF);

    printf("FMC started. Press function keys to switch pages.\n");

    // ---- 6. 主循环 ----
    bool running = true;
    int  hover_idx = -1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;

            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w, h;
                SDL_GetWindowSize(renderer.window, &w, &h);
                renderer.update_scale(w, h);
            }

            if (e.type == SDL_MOUSEMOTION) {
                int mx = e.motion.x, my = e.motion.y, w, h;
                SDL_GetWindowSize(renderer.window, &w, &h);
                int idx = fmc_hit_test(mx, my, renderer.scale, w, h);
                if (idx != hover_idx) {
                    fmc_update_hover(idx);
                    hover_idx = idx;
                }
            }

            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx = e.button.x, my = e.button.y, w, h;
                SDL_GetWindowSize(renderer.window, &w, &h);
                int idx = fmc_hit_test(mx, my, renderer.scale, w, h);
                if (idx >= 0) {
                    printf("[FMC] Click: %s (page: %s)\n",
                           fmc_buttons[idx].label,
                           g_pages[g_screen.current_page].title);
                    fmc_handle_click(idx);
                }
            }

            if (e.type == SDL_TEXTINPUT && e.text.text[0] >= 32) {
                g_screen.type_char(e.text.text[0]);
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE) {
                g_screen.backspace();
            }
        }

        // ---- 7. 渲染 ----
        renderer.clear();
        renderer.draw_bg();

        for (int i = 0; i < FMC_KEY_COUNT; i++) {
            FMCButton& btn = fmc_buttons[i];
            if (!btn.label) continue;
            if (btn.shape == FMC_SHAPE_CIRCLE)
                renderer.draw_btn_circle(btn.rect.x + btn.rect.w/2,
                    btn.rect.y + btn.rect.h/2, btn.radius, btn.state);
            else
                renderer.draw_btn_rect(btn.rect.x, btn.rect.y,
                    btn.rect.w, btn.rect.h, btn.state);
        }

        fmc_draw_screen(renderer);

        char buf[48];
        snprintf(buf, sizeof(buf), "Page: %s", g_pages[g_screen.current_page].title);
        renderer.draw_text(10, 960, buf, Color::FMC_GREEN, true);

        renderer.present();
        SDL_Delay(16);
    }

    // ---- 8. 资源清理 ----
    printf("FMC stopped.\n");
    g_shm.close();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
