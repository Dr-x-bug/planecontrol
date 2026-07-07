#pragma once
#include <SDL2/SDL.h>

constexpr int FMC_BASE_W      = 638;
constexpr int FMC_BASE_H      = 998;
constexpr int WIN_W           = 830;
constexpr int WIN_H           = 1298;
constexpr int SCREEN_WIDTH    = 500;
constexpr int SCREEN_HEIGHT   = 24;
constexpr int FUNCTION_WIDTH  = 68;
constexpr int FUNCTION_HEIGHT = 38;
constexpr int LETTER_LENGTH   = 52;
constexpr int NUMB_R          = 26;

namespace Color {
    constexpr SDL_Color FMC_TEXT    = {255, 255, 255, 255};
    constexpr SDL_Color FMC_GREEN   = {  0, 255,   0, 255};
    constexpr SDL_Color FMC_CYAN    = {  0, 220, 220, 255};
    constexpr SDL_Color FMC_AMBER   = {255, 180,   0, 255};
    constexpr SDL_Color FMC_MAGENTA = {255,   0, 255, 255};
}

enum {
    FMC_STATE_NORMAL  = 0,
    FMC_STATE_HOVER   = 1,
    FMC_STATE_PRESSED = 2,
    FMC_STATE_ACTIVE  = 4,
};

enum FMCShape { FMC_SHAPE_NONE, FMC_SHAPE_RECT, FMC_SHAPE_CIRCLE };
