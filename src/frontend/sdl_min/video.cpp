/*
    video.cpp - Renderizado de las dos pantallas del DS con SDL2
    Parte del fork RG DS Plus de melonDS.
    Licencia: GPLv3
*/

#include "video.h"
#include "Platform.h"

#include <cstdio>
#include <cstring>

namespace melonDS {
namespace FrontendSDL {

static constexpr int DS_WIDTH  = 256;
static constexpr int DS_HEIGHT = 192;

Video::Video() {}

Video::~Video()
{
    DeInit();
}

bool Video::Init(int scale)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        Platform::Log(Platform::LogLevel::Error,
            "SDL_InitSubSystem(VIDEO) fallo: %s", SDL_GetError());
        return false;
    }

    num_displays_ = SDL_GetNumVideoDisplays();
    Platform::Log(Platform::LogLevel::Info,
        "Displays detectados: %d", num_displays_);

    for (int i = 0; i < num_displays_; i++) {
        SDL_Rect bounds;
        SDL_GetDisplayBounds(i, &bounds);
        Platform::Log(Platform::LogLevel::Info,
            "  Display %d: %dx%d @ (%d,%d)",
            i, bounds.w, bounds.h, bounds.x, bounds.y);
    }

    if (num_displays_ >= 2) {
        dual_display_ = true;
        if (!InitDisplay(0, &win_top_, &ren_top_, &tex_top_, scale,
                         "melonDS DS Top Screen")) {
            return false;
        }
        if (!InitDisplay(1, &win_bot_, &ren_bot_, &tex_bot_, scale,
                         "melonDS DS Bottom Screen")) {
            return false;
        }
        Platform::Log(Platform::LogLevel::Info,
            "Modo DUAL: dos ventanas fullscreen en displays 0 y 1");
    }
    else {
        dual_display_ = false;
        int win_w = DS_WIDTH * scale;
        int win_h = DS_HEIGHT * scale * 2;

        win_top_ = SDL_CreateWindow("melonDS (Single Window)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            win_w, win_h, 0);
        if (!win_top_) {
            Platform::Log(Platform::LogLevel::Error,
                "SDL_CreateWindow fallo: %s", SDL_GetError());
            return false;
        }

        ren_top_ = SDL_CreateRenderer(win_top_, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren_top_) {
            Platform::Log(Platform::LogLevel::Error,
                "SDL_CreateRenderer fallo: %s", SDL_GetError());
            return false;
        }

        tex_top_ = SDL_CreateTexture(ren_top_,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            DS_WIDTH, DS_HEIGHT * 2);
        if (!tex_top_) {
            Platform::Log(Platform::LogLevel::Error,
                "SDL_CreateTexture fallo: %s", SDL_GetError());
            return false;
        }

        Platform::Log(Platform::LogLevel::Info,
            "Modo SINGLE: una ventana %dx%d con dos pantallas apiladas",
            win_w, win_h);
    }

    return true;
}

bool Video::InitDisplay(int index, SDL_Window** win, SDL_Renderer** ren,
                        SDL_Texture** tex, int scale, const char* title)
{
    (void)scale;
    SDL_Rect bounds;
    if (SDL_GetDisplayBounds(index, &bounds) != 0) {
        Platform::Log(Platform::LogLevel::Error,
            "SDL_GetDisplayBounds(%d) fallo: %s", index, SDL_GetError());
        return false;
    }

    *win = SDL_CreateWindow(title,
        bounds.x, bounds.y, bounds.w, bounds.h,
        SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!*win) {
        Platform::Log(Platform::LogLevel::Error,
            "SDL_CreateWindow(%s) fallo: %s", title, SDL_GetError());
        return false;
    }

    *ren = SDL_CreateRenderer(*win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*ren) {
        Platform::Log(Platform::LogLevel::Warn,
            "SDL_CreateRenderer acelerado fallo, probando software: %s",
            SDL_GetError());
        *ren = SDL_CreateRenderer(*win, -1, SDL_RENDERER_SOFTWARE);
        if (!*ren) {
            Platform::Log(Platform::LogLevel::Error,
                "SDL_CreateRenderer software fallo: %s", SDL_GetError());
            return false;
        }
    }

    *tex = SDL_CreateTexture(*ren,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        DS_WIDTH, DS_HEIGHT);
    if (!*tex) {
        Platform::Log(Platform::LogLevel::Error,
            "SDL_CreateTexture fallo: %s", SDL_GetError());
        return false;
    }

    SDL_SetTextureScaleMode(*tex, SDL_ScaleModeNearest);
    SDL_SetTextureBlendMode(*tex, SDL_BLENDMODE_NONE);

    Platform::Log(Platform::LogLevel::Info,
        "Display %d listo: %dx%d", index, bounds.w, bounds.h);

    return true;
}

void Video::PresentFrame(const uint32_t* top, const uint32_t* bottom)
{
    if (dual_display_) {
        RenderScreen(ren_top_, tex_top_, top);
        RenderScreen(ren_bot_, tex_bot_, bottom);
    } else {
        static uint32_t combined[DS_WIDTH * DS_HEIGHT * 2];
        memcpy(combined, top, DS_WIDTH * DS_HEIGHT * sizeof(uint32_t));
        memcpy(combined + DS_WIDTH * DS_HEIGHT, bottom,
               DS_WIDTH * DS_HEIGHT * sizeof(uint32_t));

        SDL_UpdateTexture(tex_top_, nullptr, combined,
                          DS_WIDTH * sizeof(uint32_t));

        SDL_RenderClear(ren_top_);
        SDL_RenderCopy(ren_top_, tex_top_, nullptr, nullptr);
        SDL_RenderPresent(ren_top_);
    }
}

void Video::RenderScreen(SDL_Renderer* ren, SDL_Texture* tex,
                         const uint32_t* pixels)
{
    SDL_UpdateTexture(tex, nullptr, pixels, DS_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, nullptr, nullptr);
    SDL_RenderPresent(ren);
}

void Video::DeInit()
{
    if (tex_top_) { SDL_DestroyTexture(tex_top_); tex_top_ = nullptr; }
    if (tex_bot_) { SDL_DestroyTexture(tex_bot_); tex_bot_ = nullptr; }
    if (ren_top_) { SDL_DestroyRenderer(ren_top_); ren_top_ = nullptr; }
    if (ren_bot_) { SDL_DestroyRenderer(ren_bot_); ren_bot_ = nullptr; }
    if (win_top_) { SDL_DestroyWindow(win_top_); win_top_ = nullptr; }
    if (win_bot_) { SDL_DestroyWindow(win_bot_); win_bot_ = nullptr; }
}

} // namespace FrontendSDL
} // namespace melonDS
