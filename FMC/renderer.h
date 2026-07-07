#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <string>
#include "config.h"

struct FMCRenderer {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* sdl_rend = nullptr;
    TTF_Font*     font     = nullptr;
    TTF_Font*     font_sm  = nullptr;
    SDL_Texture*  fmc_tex  = nullptr;
    float         scale    = 1.3f;

    bool init() {
        SDL_Init(SDL_INIT_VIDEO); TTF_Init(); IMG_Init(IMG_INIT_PNG);
        window = SDL_CreateWindow("FMC", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WIN_W, WIN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        sdl_rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        font    = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 16);
        font_sm = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 13);
        SDL_Surface* s = IMG_Load("assets/fmc.png");
        if (s) { fmc_tex = SDL_CreateTextureFromSurface(sdl_rend, s); SDL_FreeSurface(s); }
        return window && sdl_rend && font && fmc_tex;
    }

    void update_scale(int ww, int wh) {
        float sx=ww/(float)FMC_BASE_W, sy=wh/(float)FMC_BASE_H;
        scale = (sx<sy)?sx:sy;
    }
    void clear() { SDL_SetRenderDrawColor(sdl_rend,0,0,0,255); SDL_RenderClear(sdl_rend); }
    void present() { SDL_RenderPresent(sdl_rend); }

    int to_sx(int x) { int wx,wy; SDL_GetWindowSize(window,&wx,&wy); return (int)(x*scale)+(wx-(int)(FMC_BASE_W*scale))/2; }
    int to_sy(int y) { int wx,wy; SDL_GetWindowSize(window,&wx,&wy); return (int)(y*scale)+(wy-(int)(FMC_BASE_H*scale))/2; }
    int to_sw(int w) { return (int)(w*scale); }
    int to_sh(int h) { return (int)(h*scale); }

    void draw_bg() {
        int dw=to_sw(FMC_BASE_W), dh=to_sh(FMC_BASE_H);
        SDL_Rect d={to_sx(0), to_sy(0), dw, dh};
        SDL_RenderCopy(sdl_rend, fmc_tex, nullptr, &d);
    }

    // 绿色FMC风格文字
    void draw_text(int x, int y, const std::string& txt, SDL_Color c, bool sm=false) {
        if (txt.empty()) return;
        TTF_Font* f = sm ? font_sm : font;
        SDL_Surface* s = TTF_RenderUTF8_Blended(f, txt.c_str(), c);
        if (!s) return;
        SDL_Texture* t = SDL_CreateTextureFromSurface(sdl_rend, s);
        SDL_Rect d = {to_sx(x), to_sy(y), (int)(s->w*scale), (int)(s->h*scale)};
        SDL_RenderCopy(sdl_rend, t, nullptr, &d);
        SDL_FreeSurface(s); SDL_DestroyTexture(t);
    }

    // 填充矩形 (用于屏幕背景)
    void fill_rect(int x, int y, int w, int h, SDL_Color c) {
        SDL_Rect r = {to_sx(x), to_sy(y), to_sw(w), to_sh(h)};
        SDL_SetRenderDrawColor(sdl_rend, c.r, c.g, c.b, c.a);
        SDL_RenderFillRect(sdl_rend, &r);
    }

    // 矩形按钮: 正常透明, 悬停亮框, 按下亮黄框
    void draw_btn_rect(int x, int y, int w, int h, int state) {
        if (state == FMC_STATE_NORMAL) return; // 正常态不绘制
        int xs=to_sx(x), ys=to_sy(y), ws=to_sw(w), hs=to_sh(h), r=to_sw(4);
        int alpha = 200;
        if (state & FMC_STATE_HOVER) {
            roundedRectangleRGBA(sdl_rend, xs, ys, xs+ws, ys+hs, r, 100,200,255,alpha);
            roundedRectangleRGBA(sdl_rend, xs+1, ys+1, xs+ws-1, ys+hs-1, r, 100,200,255,alpha/2);
        }
        if (state & FMC_STATE_PRESSED) {
            roundedRectangleRGBA(sdl_rend, xs, ys, xs+ws, ys+hs, r, 255,255,100,alpha);
        }
        if (state & FMC_STATE_ACTIVE) {
            roundedRectangleRGBA(sdl_rend, xs, ys, xs+ws, ys+hs, r, 0,255,0,alpha);
            roundedRectangleRGBA(sdl_rend, xs+1, ys+1, xs+ws-1, ys+hs-1, r, 0,255,0,alpha/2);
        }
    }

    // 圆形按钮: 正常透明, 悬停亮圈
    void draw_btn_circle(int cx, int cy, int r, int state) {
        if (state == FMC_STATE_NORMAL) return;
        int xc=to_sx(cx), yc=to_sy(cy), rs=to_sw(r);
        int alpha = 200;
        if (state & FMC_STATE_HOVER) {
            aacircleRGBA(sdl_rend, xc, yc, rs, 100,200,255,alpha);
            aacircleRGBA(sdl_rend, xc, yc, rs-1, 100,200,255,alpha/2);
        }
        if (state & FMC_STATE_PRESSED) {
            aacircleRGBA(sdl_rend, xc, yc, rs, 255,255,100,alpha);
        }
    }

    ~FMCRenderer() {
        if (fmc_tex) SDL_DestroyTexture(fmc_tex);
        if (font_sm) TTF_CloseFont(font_sm);
        if (font) TTF_CloseFont(font);
        if (sdl_rend) SDL_DestroyRenderer(sdl_rend);
        if (window) SDL_DestroyWindow(window);
        // IMG_Quit() / TTF_Quit() / SDL_Quit() 由主程序统一调用
    }
};
