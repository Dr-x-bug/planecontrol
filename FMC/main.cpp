#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include "config.h"
#include "renderer.h"
#include "fmc_ui.h"
#include "fmc_pages.h"
#include "fmc_route.h"
#include "../shared_mem.h"
#include "../fmc_shm_sync.h"

// 全局共享内存IPC
SharedMemoryIPC g_shm;

// 全局同步回调实例 (在fmc_shm_sync.h中声明)
RouteSyncCallback g_route_sync_cb = nullptr;

// 将g_route中的航路数据写入共享内存
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

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    FMCRenderer renderer;
    if (!renderer.init()) { printf("FMC init failed!\n"); return 1; }

    // 加载导航数据到AVL树 + FMS飞行计划到航道数组
    printf("[FMC] Loading nav data...\n");
    load_navdata_avl("assets/earth_nav.dat", "assets/earth_fix.dat", "assets/apt.dat");
    load_route_from_fms("assets/KSEAKBFI.fms");
    printf("[FMC] Route: %d waypoints, %d pages\n", g_route.count, g_route.total_pages());

    // 初始化共享内存 (FMC作为创建方)
    printf("[FMC] Initializing shared memory IPC...\n");
    if (g_shm.create()) {
        fmc_sync_route_to_shm();  // 同步初始航路到共享内存
    }

    // 注册航路同步回调 (供fmc_buttons调用)
    g_route_sync_cb = fmc_sync_route_to_shm;

    fmc_buttons_init();
    fmc_switch_page(PAGE_INDEX);

    printf("FMC started. Press function keys to switch pages.\n");

    bool running = true;
    int hover_idx = -1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;

            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w, h; SDL_GetWindowSize(renderer.window, &w, &h);
                renderer.update_scale(w, h);
            }

            if (e.type == SDL_MOUSEMOTION) {
                int mx=e.motion.x, my=e.motion.y, w, h;
                SDL_GetWindowSize(renderer.window, &w, &h);
                int idx = fmc_hit_test(mx, my, renderer.scale, w, h);
                if (idx != hover_idx) { fmc_update_hover(idx); hover_idx = idx; }
            }

            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx=e.button.x, my=e.button.y, w, h;
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

        // === 渲染 ===
        renderer.clear();
        renderer.draw_bg();

        // 按键高亮
        for (int i = 0; i < FMC_KEY_COUNT; i++) {
            FMCButton& btn = fmc_buttons[i];
            if (!btn.label) continue;
            if (btn.shape == FMC_SHAPE_CIRCLE)
                renderer.draw_btn_circle(btn.rect.x+btn.rect.w/2, btn.rect.y+btn.rect.h/2, btn.radius, btn.state);
            else
                renderer.draw_btn_rect(btn.rect.x, btn.rect.y, btn.rect.w, btn.rect.h, btn.state);
        }

        // 屏幕绘制 (统一入口)
        fmc_draw_screen(renderer);

        // 当前页面指示器
        char buf[48];
        snprintf(buf, 48, "Page: %s", g_pages[g_screen.current_page].title);
        renderer.draw_text(10, 960, buf, Color::FMC_GREEN, true);

        renderer.present();
        SDL_Delay(16);
    }

    printf("FMC stopped.\n");
    g_shm.close();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
