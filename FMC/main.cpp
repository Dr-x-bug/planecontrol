#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include "config.h"
#include "renderer.h"
#include "fmc_ui.h"
#include "fmc_pages.h"
#include "fmc_route.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    FMCRenderer renderer;
    if (!renderer.init()) { printf("FMC init failed!\n"); return 1; }

    // 加载导航数据到AVL树 + FMS飞行计划到航道数组
    printf("[FMC] Loading nav data...\n");
    load_navdata_avl("assets/earth_nav.dat", "assets/earth_fix.dat", "assets/apt.dat");
    load_route_from_fms("assets/KSEAKBFI.fms");
    printf("[FMC] Route: %d waypoints, %d pages\n", g_route.count, g_route.total_pages());

    fmc_buttons_init();
    fmc_switch_page(PAGE_INIT_REF);

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
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
