#pragma once
#include <SDL2/SDL.h>

// === Window ===
constexpr int WIN_W = 751;
constexpr int WIN_H = 721;

// === ND MAP Colors (from result image) ===
namespace Color {
    constexpr SDL_Color BG_BLACK   = {  0,   0,   0, 255};
    constexpr SDL_Color WHITE      = {255, 255, 255, 255};
    constexpr SDL_Color GREEN_RTE  = { 33, 132,  19, 255};   // route line
    constexpr SDL_Color GREEN_LT   = {160, 222, 164, 255};   // light green
    constexpr SDL_Color MAGENTA_WP = {223, 124, 237, 255};   // waypoints
    constexpr SDL_Color PURPLE     = { 96,  34,  92, 255};   // route shadow
    constexpr SDL_Color BLUE_AC    = {  0,   0, 255, 255};   // aircraft
    constexpr SDL_Color GRAY       = {100, 100, 100, 255};
    constexpr SDL_Color CYAN       = {  0, 200, 200, 255};
    constexpr SDL_Color YELLOW     = {255, 255,   0, 255};
}

// === ND Display Params ===
constexpr int   ND_CX      = WIN_W / 2;
constexpr int   ND_CY      = WIN_H / 2;
constexpr int   ND_RADIUS  = 300;
constexpr double NM_PER_PX = 0.0667;  // ~20nm range at 300px radius
