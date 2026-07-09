#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include "config.h"

struct NDRenderer {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* sdl_rend = nullptr;
    TTF_Font*     font     = nullptr;
    TTF_Font*     font_sm  = nullptr;

    bool init() {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        window = SDL_CreateWindow("ND - MAP Mode",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            ND_WIN_W, ND_WIN_H, SDL_WINDOW_SHOWN);
        sdl_rend = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        font    = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 18);
        font_sm = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 14);
        return window && sdl_rend && font;
    }

    void clear() {
        SDL_SetRenderDrawColor(sdl_rend, 0, 0, 0, 255);
        SDL_RenderClear(sdl_rend);
    }
    void present() { SDL_RenderPresent(sdl_rend); }

    void set_color(SDL_Color c) {
        SDL_SetRenderDrawColor(sdl_rend, c.r, c.g, c.b, c.a);
    }

    void draw_text(int x, int y, const std::string& txt, SDL_Color c, bool small = false) {
        if (txt.empty()) return;
        TTF_Font* f = small ? font_sm : font;
        SDL_Surface* surf = TTF_RenderText_Blended(f, txt.c_str(), c);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_rend, surf);
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(sdl_rend, tex, nullptr, &dst);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }

    void draw_text_center(int cx, int y, const std::string& txt, SDL_Color c, bool small = false) {
        if (txt.empty()) return;
        TTF_Font* f = small ? font_sm : font;
        SDL_Surface* surf = TTF_RenderText_Blended(f, txt.c_str(), c);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_rend, surf);
        SDL_Rect dst = {cx - surf->w/2, y, surf->w, surf->h};
        SDL_RenderCopy(sdl_rend, tex, nullptr, &dst);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }

    ~NDRenderer() {
        if (font_sm) TTF_CloseFont(font_sm);
        if (font)    TTF_CloseFont(font);
        if (sdl_rend) SDL_DestroyRenderer(sdl_rend);
        if (window)   SDL_DestroyWindow(window);
        // TTF_Quit() / SDL_Quit() 由主程序统一调用
    }
};
