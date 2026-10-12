/*
    video.cpp - Renderizado de las dos pantallas del DS
    Modo dual: SDL_Renderer (software) o EGL+GLES (hardware)
    Parte del fork RG DS Plus de melonDS.
    Licencia: GPLv3
*/

#include "video.h"
#include "Platform.h"

#include <cstdio>
#include <cstring>

#ifdef MELONDS_GLES
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "frontend/glad-gles/glad.h"
#endif

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

    Platform::Log(Platform::LogLevel::Info,
        "SDL video driver: %s", SDL_GetCurrentVideoDriver());

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
            win_w, win_h, SDL_WINDOW_OPENGL);
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

    // Forzar mostrar las ventanas antes de crear el contexto EGL
    // (necesario en Wayland: el wl_surface debe estar mapeado)
    if (win_top_) SDL_ShowWindow(win_top_);
    if (win_bot_) SDL_ShowWindow(win_bot_);
    SDL_PumpEvents();
    SDL_Delay(150);

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
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL);
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

// ============================================================
// EGL + GLES
// ============================================================

#ifdef MELONDS_GLES

static const char* g_vertex_shader =
    "#version 310 es\n"
    "in vec2 vPosition;\n"
    "in vec2 vTexcoord;\n"
    "out vec2 texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(vPosition, 0.0, 1.0);\n"
    "    texcoord = vTexcoord;\n"
    "}\n";

static const char* g_fragment_shader =
    "#version 310 es\n"
    "precision highp float;\n"
    "uniform sampler2DArray uTex;\n"
    "uniform float uLayer;\n"
    "in vec2 texcoord;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = texture(uTex, vec3(texcoord, uLayer));\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        Platform::Log(Platform::LogLevel::Error,
            "Shader compile error: %s", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

bool Video::InitGLProgram()
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, g_vertex_shader);
    if (!vs) return false;
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, g_fragment_shader);
    if (!fs) { glDeleteShader(vs); return false; }

    gl_program_ = glCreateProgram();
    glAttachShader(gl_program_, vs);
    glAttachShader(gl_program_, fs);
    glLinkProgram(gl_program_);
    GLint ok = 0;
    glGetProgramiv(gl_program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(gl_program_, sizeof(log), nullptr, log);
        Platform::Log(Platform::LogLevel::Error,
            "Program link error: %s", log);
        return false;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    // VAO + VBO con quad fullscreen
    static const float verts[] = {
        // x, y, u, v
        -1, -1, 0, 1,
         1, -1, 1, 1,
        -1,  1, 0, 0,
         1,  1, 1, 0,
    };
    glGenVertexArrays(1, &gl_vao_);
    glGenBuffers(1, &gl_vbo_);
    glBindVertexArray(gl_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    GLint pos = glGetAttribLocation(gl_program_, "vPosition");
    GLint tex = glGetAttribLocation(gl_program_, "vTexcoord");
    if (pos >= 0) {
        glEnableVertexAttribArray(pos);
        glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    }
    if (tex >= 0) {
        glEnableVertexAttribArray(tex);
        glVertexAttribPointer(tex, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    }
    glBindVertexArray(0);

    Platform::Log(Platform::LogLevel::Info, "InitGLProgram: OK");
    return true;
}

bool Video::InitEGL()
{
    if (!win_top_) {
        Platform::Log(Platform::LogLevel::Error, "InitEGL: no hay ventana");
        return false;
    }

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) {
        Platform::Log(Platform::LogLevel::Error, "eglGetDisplay fallo");
        return false;
    }
    egl_display_ = dpy;

    EGLint major, minor;
    if (!eglInitialize(dpy, &major, &minor)) {
        Platform::Log(Platform::LogLevel::Error, "eglInitialize fallo");
        return false;
    }
    Platform::Log(Platform::LogLevel::Info,
        "EGL %d.%d inicializado", major, minor);

    const EGLint cfg_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint num_cfg = 0;
    if (!eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &num_cfg) || num_cfg == 0) {
        Platform::Log(Platform::LogLevel::Error, "eglChooseConfig fallo");
        return false;
    }
    Platform::Log(Platform::LogLevel::Info, "eglChooseConfig: OK");

    const EGLint ctx_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };

    // Contexto y surface para pantalla superior
    EGLContext ctx_top = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
    if (ctx_top == EGL_NO_CONTEXT) {
        Platform::Log(Platform::LogLevel::Error, "eglCreateContext (top) fallo");
        return false;
    }
    Platform::Log(Platform::LogLevel::Info, "eglCreateContext (top): OK");
    EGLNativeWindowType nwt_top = (EGLNativeWindowType)win_top_;
    EGLSurface surf_top = eglCreateWindowSurface(dpy, cfg, nwt_top, nullptr);
    if (surf_top == EGL_NO_SURFACE) {
        Platform::Log(Platform::LogLevel::Error, "eglCreateWindowSurface (top) fallo");
        return false;
    }
    Platform::Log(Platform::LogLevel::Info, "eglCreateWindowSurface (top): OK");

    egl_context_top_ = ctx_top;
    egl_surface_top_ = surf_top;

    // Contexto y surface para pantalla inferior (si dual)
    if (dual_display_ && win_bot_) {
        EGLContext ctx_bot = eglCreateContext(dpy, cfg, ctx_top, ctx_attribs);
        EGLNativeWindowType nwt_bot = (EGLNativeWindowType)win_bot_;
        EGLSurface surf_bot = eglCreateWindowSurface(dpy, cfg, nwt_bot, nullptr);
        if (ctx_bot == EGL_NO_CONTEXT || surf_bot == EGL_NO_SURFACE) {
            Platform::Log(Platform::LogLevel::Warn,
                "eglCreateContext/Surface (bottom) fallo, modo single");
            egl_context_bot_ = nullptr;
            egl_surface_bot_ = nullptr;
        } else {
            egl_context_bot_ = ctx_bot;
            egl_surface_bot_ = surf_bot;
        }
    }

    // Hacer current el contexto superior y cargar glad
    if (!eglMakeCurrent(dpy, surf_top, surf_top, ctx_top)) {
        Platform::Log(Platform::LogLevel::Error, "eglMakeCurrent (top) fallo");
        return false;
    }
    Platform::Log(Platform::LogLevel::Info, "eglMakeCurrent: OK");

    if (!gladLoadGLES2Loader((GLADloadproc)eglGetProcAddress)) {
        Platform::Log(Platform::LogLevel::Error, "gladLoadGLES2 fallo");
        return false;
    }
    Platform::Log(Platform::LogLevel::Info, "gladLoadGLES2Loader: OK");
    Platform::Log(Platform::LogLevel::Info,
        "GL_VERSION: %s", (const char*)glGetString(GL_VERSION));
    Platform::Log(Platform::LogLevel::Info,
        "GL_RENDERER: %s", (const char*)glGetString(GL_RENDERER));

    if (!InitGLProgram()) {
        Platform::Log(Platform::LogLevel::Error, "InitGLProgram fallo");
        return false;
    }

    egl_initialized_ = true;
    Platform::Log(Platform::LogLevel::Info, "InitEGL: OK");
    return true;
}

void Video::PresentGLTexture(unsigned int tex_id, int layer, int screen)
{
    if (!egl_initialized_) return;

    EGLDisplay dpy = (EGLDisplay)egl_display_;
    EGLContext ctx = (screen == 0)
        ? (EGLContext)egl_context_top_
        : (EGLContext)egl_context_bot_;
    EGLSurface surf = (screen == 0)
        ? (EGLSurface)egl_surface_top_
        : (EGLSurface)egl_surface_bot_;

    if (ctx == nullptr || surf == nullptr) return;

    eglMakeCurrent(dpy, surf, surf, ctx);

    int win_w = 0, win_h = 0;
    SDL_Window* win = (screen == 0) ? win_top_ : win_bot_;
    SDL_GetWindowSize(win, &win_w, &win_h);
    glViewport(0, 0, win_w, win_h);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gl_program_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);
    GLint uloc_tex = glGetUniformLocation(gl_program_, "uTex");
    GLint uloc_layer = glGetUniformLocation(gl_program_, "uLayer");
    if (uloc_tex >= 0) glUniform1i(uloc_tex, 0);
    if (uloc_layer >= 0) glUniform1f(uloc_layer, (float)layer);

    glBindVertexArray(gl_vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void Video::SwapEGL(int screen)
{
    if (!egl_initialized_) return;
    EGLDisplay dpy = (EGLDisplay)egl_display_;
    EGLSurface surf = (screen == 0)
        ? (EGLSurface)egl_surface_top_
        : (EGLSurface)egl_surface_bot_;
    if (surf == nullptr) return;
    eglSwapBuffers(dpy, surf);
}

#else
// Stubs si no esta MELONDS_GLES
bool Video::InitGLProgram() { return false; }
bool Video::InitEGL() { return false; }
void Video::PresentGLTexture(unsigned int, int, int) {}
void Video::SwapEGL(int) {}
#endif

// ============================================================
// SDL_Renderer (fallback para SoftRenderer)
// ============================================================

void Video::PresentFrame(const uint32_t* top, const uint32_t* bottom)
{
    if (!top || !bottom) return;

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

        int win_w = 0, win_h = 0;
        SDL_GetWindowSize(win_top_, &win_w, &win_h);
        if (win_w <= 0) win_w = 512;
        if (win_h <= 0) win_h = 768;

        SDL_Rect dst = { 0, 0, win_w, win_h };

        SDL_RenderClear(ren_top_);
        SDL_RenderCopy(ren_top_, tex_top_, nullptr, &dst);
        SDL_RenderPresent(ren_top_);
    }
}

void Video::RenderScreen(SDL_Renderer* ren, SDL_Texture* tex,
                         const uint32_t* pixels)
{
    SDL_UpdateTexture(tex, nullptr, pixels, DS_WIDTH * sizeof(uint32_t));

    int win_w = 0, win_h = 0;
    SDL_Window* win = SDL_RenderGetWindow(ren);
    if (win) SDL_GetWindowSize(win, &win_w, &win_h);
    if (win_w <= 0) win_w = 640;
    if (win_h <= 0) win_h = 480;

    SDL_Rect dst = { 0, 0, win_w, win_h };

    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);
}

void Video::SetScaleMode(int mode)
{
    if (mode == scale_mode_) return;
    scale_mode_ = mode;
    // Por ahora: no aplicamos cambios al renderer (eso ya lo hace el GLRenderer)
    Platform::Log(Platform::LogLevel::Info,
        "Video: modo de escalado cambiado a %d", mode);
}

void Video::DeInit()
{
    // EGL
    if (egl_initialized_) {
        EGLDisplay dpy = (EGLDisplay)egl_display_;
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_context_bot_) eglDestroyContext(dpy, (EGLContext)egl_context_bot_);
        if (egl_context_top_) eglDestroyContext(dpy, (EGLContext)egl_context_top_);
        if (egl_surface_bot_) eglDestroySurface(dpy, (EGLSurface)egl_surface_bot_);
        if (egl_surface_top_) eglDestroySurface(dpy, (EGLSurface)egl_surface_top_);
        eglTerminate(dpy);
        egl_initialized_ = false;
    }

    if (tex_top_) { SDL_DestroyTexture(tex_top_); tex_top_ = nullptr; }
    if (tex_bot_) { SDL_DestroyTexture(tex_bot_); tex_bot_ = nullptr; }
    if (ren_top_) { SDL_DestroyRenderer(ren_top_); ren_top_ = nullptr; }
    if (ren_bot_) { SDL_DestroyRenderer(ren_bot_); ren_bot_ = nullptr; }
    if (win_top_) { SDL_DestroyWindow(win_top_); win_top_ = nullptr; }
    if (win_bot_) { SDL_DestroyWindow(win_bot_); win_bot_ = nullptr; }
}

} // namespace FrontendSDL
} // namespace melonDS
