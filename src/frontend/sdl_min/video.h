#ifndef SDL_MIN_VIDEO_H
#define SDL_MIN_VIDEO_H

#include <SDL2/SDL.h>
#include <cstdint>

namespace melonDS {
namespace FrontendSDL {

class Video
{
public:
    Video();
    ~Video();
    bool Init(int scale = 2);
    void DeInit();
    void PresentFrame(const uint32_t* top, const uint32_t* bottom);
    SDL_Window* GetWindowTop() const { return win_top_; }
    SDL_Window* GetWindowBottom() const { return win_bot_; }
    int GetNumDisplays() const { return num_displays_; }
    bool IsDualDisplay() const { return dual_display_; }

private:
    SDL_Window* win_top_ = nullptr;
    SDL_Window* win_bot_ = nullptr;
    SDL_Renderer* ren_top_ = nullptr;
    SDL_Renderer* ren_bot_ = nullptr;
    SDL_Texture* tex_top_ = nullptr;
    SDL_Texture* tex_bot_ = nullptr;
    int num_displays_ = 0;
    bool dual_display_ = false;

    bool InitDisplay(int index, SDL_Window** win, SDL_Renderer** ren,
                     SDL_Texture** tex, int scale, const char* title);
    void RenderScreen(SDL_Renderer* ren, SDL_Texture* tex,
                      const uint32_t* pixels);
};

} // namespace FrontendSDL
} // namespace melonDS

#endif
