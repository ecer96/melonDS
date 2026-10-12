#ifndef SDL_MIN_VIDEO_H
#define SDL_MIN_VIDEO_H

#include <SDL2/SDL.h>
#include <cstdint>

namespace melonDS {
namespace FrontendSDL {

enum ScaleMode {
    SCALE_NEAREST = 0,
    SCALE_LINEAR  = 1,
    SCALE_INTEGER = 2,
};

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
    void SetScaleMode(int mode);
    int  GetScaleMode() const { return scale_mode_; }

    // GL: EGL + GLES
    bool InitEGL();
    bool HasEGL() const { return egl_initialized_; }
    void PresentGLTexture(unsigned int tex_id, int layer, int screen);
    void SwapEGL(int screen);

private:
    SDL_Window* win_top_ = nullptr;
    SDL_Window* win_bot_ = nullptr;
    SDL_Renderer* ren_top_ = nullptr;
    SDL_Renderer* ren_bot_ = nullptr;
    SDL_Texture* tex_top_ = nullptr;
    SDL_Texture* tex_bot_ = nullptr;
    int num_displays_ = 0;
    bool dual_display_ = false;
    int scale_mode_ = SCALE_NEAREST;

    // EGL/GLES
    bool egl_initialized_ = false;
    void* egl_display_ = nullptr;
    void* egl_context_top_ = nullptr;
    void* egl_context_bot_ = nullptr;
    void* egl_surface_top_ = nullptr;
    void* egl_surface_bot_ = nullptr;
    unsigned int gl_program_ = 0;
    unsigned int gl_vao_ = 0;
    unsigned int gl_vbo_ = 0;

    bool InitDisplay(int index, SDL_Window** win, SDL_Renderer** ren,
                     SDL_Texture** tex, int scale, const char* title);
    void RenderScreen(SDL_Renderer* ren, SDL_Texture* tex,
                      const uint32_t* pixels);
    bool InitGLProgram();
};

} // namespace FrontendSDL
} // namespace melonDS

#endif
