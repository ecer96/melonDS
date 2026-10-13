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

    // GL: SDL_GL + GLES
    bool InitEGL();
    bool HasEGL() const { return gl_mode_; }
    void PresentGLTexture(unsigned int tex_id, int layer, int screen);
    void SwapEGL(int screen);  // ahora es no-op (el swap lo hace PresentGLTexture)

private:
    SDL_Window* win_top_ = nullptr;
    SDL_Window* win_bot_ = nullptr;
    int num_displays_ = 0;
    bool dual_display_ = false;
    int scale_mode_ = SCALE_NEAREST;

    // SDL_GL
    bool gl_mode_ = false;
    SDL_GLContext gl_ctx_ = nullptr;
    unsigned int gl_program_ = 0;
    unsigned int gl_vao_ = 0;
    unsigned int gl_vbo_ = 0;

    bool InitDisplay(int index, SDL_Window** win, const char* title);
    bool InitGLProgram();
};

} // namespace FrontendSDL
} // namespace melonDS

#endif
