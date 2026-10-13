/*
    video.cpp - Renderizado de las dos pantallas del DS
    Modo dual: SDL_GL + GLES (con GLRenderer de melonDS)
    Parte del fork RG DS Plus de melonDS.
    Licencia: GPLv3
*/

#include "video.h"
#include "Platform.h"

#include <cstdio>
#include <cstring>

#ifdef MELONDS_GLES
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

    // Configurar GLES 3.0/3.2 ANTES de crear las ventanas
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_EGL, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    if (num_displays_ >= 2) {
        dual_display_ = true;
        if (!InitDisplay(0, &win_top_, "melonDS DS Top Screen")) {
            return false;
        }
        if (!InitDisplay(1, &win_bot_, "melonDS DS Bottom Screen")) {
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

        Platform::Log(Platform::LogLevel::Info,
            "Modo SINGLE: una ventana %dx%d con dos pantallas apiladas",
            win_w, win_h);
    }

    // Mostrar las ventanas antes de crear el contexto GL
    if (win_top_) SDL_ShowWindow(win_top_);
    if (win_bot_) SDL_ShowWindow(win_bot_);
    SDL_PumpEvents();
    SDL_Delay(150);

    return true;
}

bool Video::InitDisplay(int index, SDL_Window** win, const char* title)
{
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

    Platform::Log(Platform::LogLevel::Info,
        "Display %d listo: %dx%d", index, bounds.w, bounds.h);

    return true;
}

// ============================================================
// SDL_GL + GLES
// ============================================================

#ifdef MELONDS_GLES

static const char* g_vertex_shader =
    "#version 300 es\n"
    "layout(location = 0) in vec2 vPosition;\n"
    "layout(location = 1) in vec2 vTexcoord;\n"
    "out vec2 texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(vPosition, 0.0, 1.0);\n"
    "    texcoord = vTexcoord;\n"
    "}\n";

static const char* g_fragment_shader =
    "#version 300 es\n"
    "precision highp float;\n"
    "precision highp sampler2DArray;\n"
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
    if (!vs) { Platform::Log(Platform::LogLevel::Error, "[InitGLProgram] VS fallo"); return false; }
    Platform::Log(Platform::LogLevel::Info, "[InitGLProgram] VS compilado OK");

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, g_fragment_shader);
    if (!fs) { glDeleteShader(vs); Platform::Log(Platform::LogLevel::Error, "[InitGLProgram] FS fallo"); return false; }
    Platform::Log(Platform::LogLevel::Info, "[InitGLProgram] FS compilado OK");

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

    // VAO + VBO con quad fullscreen + coords de textura
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

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

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

    // Crear contexto GLES con SDL_GL
    gl_ctx_ = SDL_GL_CreateContext(win_top_);
    if (!gl_ctx_) {
        Platform::Log(Platform::LogLevel::Error,
            "SDL_GL_CreateContext fallo: %s", SDL_GetError());
        return false;
    }
    Platform::Log(Platform::LogLevel::Info, "SDL_GL_CreateContext: OK");

    if (SDL_GL_MakeCurrent(win_top_, gl_ctx_) != 0) {
        Platform::Log(Platform::LogLevel::Error,
            "SDL_GL_MakeCurrent fallo: %s", SDL_GetError());
        return false;
    }
    Platform::Log(Platform::LogLevel::Info, "SDL_GL_MakeCurrent: OK");

    SDL_GL_SetSwapInterval(0);  // sin vsync (pacing manual)

    // Cargar glad con SDL_GL_GetProcAddress
    if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress)) {
        Platform::Log(Platform::LogLevel::Error, "gladLoadGLES2Loader fallo");
        return false;
    }
    Platform::Log(Platform::LogLevel::Info,
        "GL_VERSION: %s", (const char*)glGetString(GL_VERSION));
    Platform::Log(Platform::LogLevel::Info,
        "GL_RENDERER: %s", (const char*)glGetString(GL_RENDERER));

    if (!InitGLProgram()) {
        Platform::Log(Platform::LogLevel::Error, "InitGLProgram fallo");
        return false;
    }

    gl_mode_ = true;
    Platform::Log(Platform::LogLevel::Info, "InitEGL: OK");
    return true;
}

void Video::PresentGLTexture(unsigned int tex_id, int layer, int screen)
{
    if (!gl_mode_) return;

    SDL_Window* win = (screen == 0) ? win_top_ : win_bot_;
    if (!win) return;

    SDL_GL_MakeCurrent(win, gl_ctx_);

    // FIX CRITICO: el GLRenderer deja un FBO bindeado.
    // Sin esto, el clear y el draw caen en el FBO del renderer, no en la ventana.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // FIX: resetear estado que el GLRenderer puede haber dejado sucio
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(win, &win_w, &win_h);
    glViewport(0, 0, win_w, win_h);

    static int viewport_log = 0;
    if (viewport_log < 10) {
        Platform::Log(Platform::LogLevel::Info, "[PRESENT] screen=%d win=%p win_w=%d win_h=%d", screen, (void*)win, win_w, win_h);
        viewport_log++;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // NEGRO
    glClear(GL_COLOR_BUFFER_BIT);

    static int presentlogcount = 0;
    if (presentlogcount < 5) {
        Platform::Log(Platform::LogLevel::Info, "[PRESENT] screen=%d tex_id=%u win=%p ctx=%p", screen, tex_id, (void*)win, (void*)gl_ctx_);
        presentlogcount++;
    }
    glUseProgram(gl_program_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);

    // FIX: texturas sin mipmaps -> forzar NEAREST, sin filtro de mipmap
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLint uloc_tex = glGetUniformLocation(gl_program_, "uTex");
    GLint uloc_layer = glGetUniformLocation(gl_program_, "uLayer");
    if (uloc_tex >= 0) glUniform1i(uloc_tex, 0);
    if (uloc_layer >= 0) glUniform1f(uloc_layer, (float)layer);

    glBindVertexArray(gl_vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    {
        unsigned char px[4];
        glReadPixels(win_w/2, win_h/2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        static int pxlog = 0;
        if (pxlog < 5) {
            Platform::Log(Platform::LogLevel::Info, "px=%u %u %u %u", px[0], px[1], px[2], px[3]);
            pxlog++;
        }
    }

    SDL_GL_SwapWindow(win);
}

void Video::SwapEGL(int screen)
{
    (void)screen;
    // No-op: SDL_GL_SwapWindow ya se llama en PresentGLTexture
}

#else
// Stubs si no esta MELONDS_GLES
bool Video::InitGLProgram() { return false; }
bool Video::InitEGL() { return false; }
void Video::PresentGLTexture(unsigned int, int, int) {}
void Video::SwapEGL(int) {}
#endif

// ============================================================
// PresentFrame (stub: solo se usa con SoftRenderer + SDL_Renderer)
// ============================================================

void Video::PresentFrame(const uint32_t* top, const uint32_t* bottom)
{
    (void)top;
    (void)bottom;
    // No-op: el renderer GL escribe directamente a la textura del SDL_GL
    // y PresentGLTexture se encarga de presentar.
}

void Video::SetScaleMode(int mode)
{
    if (mode == scale_mode_) return;
    scale_mode_ = mode;
    Platform::Log(Platform::LogLevel::Info,
        "Video: modo de escalado cambiado a %d", mode);
}

void Video::DeInit()
{
    if (gl_ctx_) {
        SDL_GL_DeleteContext(gl_ctx_);
        gl_ctx_ = nullptr;
    }
    if (gl_program_) { glDeleteProgram(gl_program_); gl_program_ = 0; }
    if (gl_vao_) { glDeleteVertexArrays(1, &gl_vao_); gl_vao_ = 0; }
    if (gl_vbo_) { glDeleteBuffers(1, &gl_vbo_); gl_vbo_ = 0; }

    if (win_top_) { SDL_DestroyWindow(win_top_); win_top_ = nullptr; }
    if (win_bot_) { SDL_DestroyWindow(win_bot_); win_bot_ = nullptr; }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

} // namespace FrontendSDL
} // namespace melonDS
