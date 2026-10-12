/*
    main.cpp - Frontend SDL2 minimo para melonDS
    Parte del fork RG DS Plus de melonDS.
    Licencia: GPLv3
*/

#include "Platform.h"
#include "NDS.h"
#include "NDSCart.h"
#include "GPU.h"
#include "GPU_Soft.h"
#include "GPU_OpenGL.h"
#include "GPU_OpenGL.h"
#include "SPU.h"
#include "SPI_Firmware.h"

#include "video.h"
#include "input.h"
#include "audio.h"
#include "menu_commands.h"

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace melonDS {
namespace Platform {
    bool ShouldStop();
}
}

using namespace melonDS;
using namespace melonDS::FrontendSDL;

static std::unique_ptr<u8[]> LoadFile(const std::string& path, u32& out_len)
{
    Platform::FileHandle* fh = Platform::OpenFile(path, Platform::FileMode::Read);
    if (!fh) {
        Platform::Log(Platform::LogLevel::Error, "No se pudo abrir: %s", path.c_str());
        out_len = 0;
        return nullptr;
    }
    u64 len = Platform::FileLength(fh);
    if (len == 0) { Platform::CloseFile(fh); out_len = 0; return nullptr; }
    auto buf = std::make_unique<u8[]>((size_t)len);
    u64 read = Platform::FileRead(buf.get(), 1, len, fh);
    Platform::CloseFile(fh);
    if (read != len) {
        Platform::Log(Platform::LogLevel::Error, "Lectura incompleta: %s", path.c_str());
        out_len = 0;
        return nullptr;
    }
    out_len = (u32)len;
    return buf;
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 2) {
        printf("Uso: melonDS_sdl <rom.nds>\n");
        return 1;
    }

    std::string rom_path = argv[1];

    Platform::Log(Platform::LogLevel::Info, "melonDS SDL frontend - RG DS Plus fork");
    Platform::Log(Platform::LogLevel::Info, "ROM: %s", rom_path.c_str());

    // 1. Crear el emulador con firmware generado
    NDSArgs args;
    args.OutputSampleRate = 48000.0;
    args.Interpolation = AudioInterpolation::None;
    args.BitDepth = AudioBitDepth::Auto;
    args.Firmware = melonDS::Firmware(0);
    auto nds = std::make_unique<NDS>(std::move(args));

    // 2. Cargar la ROM
    u32 rom_len = 0;
    auto rom_data = LoadFile(rom_path, rom_len);
    if (!rom_data) {
        Platform::Log(Platform::LogLevel::Error, "No se pudo cargar la ROM.");
        return 1;
    }
    Platform::Log(Platform::LogLevel::Info, "ROM cargada: %u bytes", rom_len);

    auto cart = NDSCart::ParseROM(std::move(rom_data), rom_len);
    if (!cart) {
        Platform::Log(Platform::LogLevel::Error, "NDSCart::ParseROM fallo.");
        return 1;
    }
    nds->SetNDSCart(std::move(cart));

    // 4. Reset (inicializa PU, CP15, ExceptionBase, timings, JIT)
    Platform::Log(Platform::LogLevel::Info, "=== Reset ===");
    nds->Reset();

    // 5. SetupDirectBoot (copia ARM9/ARM7 a RAM, configura el cart)
    Platform::Log(Platform::LogLevel::Info, "=== SetupDirectBoot ===");
    nds->SetupDirectBoot(rom_path);

    // 6. Start (Running = true, arranca la emulacion)
    Platform::Log(Platform::LogLevel::Info, "=== Start ===");
    nds->Start();

    // 7. Init video, input, audio
    Video video;
    if (!video.Init(2)) {
        Platform::Log(Platform::LogLevel::Error, "Video::Init fallo");
        return 1;
    }

    // Init EGL/GLES
    bool use_gl = video.InitEGL();
    if (use_gl) {
        Platform::Log(Platform::LogLevel::Info, "Usando GLRenderer (compute)");
        auto glr = std::make_unique<melonDS::GLRenderer>(*nds, true);
        nds->SetRenderer(std::move(glr));
        melonDS::RendererSettings settings = {};
        settings.ScaleFactor = 2;
        settings.Threaded = false;
        settings.HiresCoordinates = true;
        settings.BetterPolygons = false;
        nds->GetRenderer().SetRenderSettings(settings);
    } else {
        Platform::Log(Platform::LogLevel::Warn, "EGL fallo, usando SoftRenderer");
        nds->SetRenderer(std::make_unique<melonDS::SoftRenderer>(*nds));
    }

    // Reset despues de setear el renderer (necesario para GLRenderer)
    nds->Reset();

    Input input;
    if (!input.Init()) {
        Platform::Log(Platform::LogLevel::Error, "Input::Init fallo");
        return 1;
    }

    Audio audio;
    if (!audio.Init(*nds)) {
        Platform::Log(Platform::LogLevel::Error, "Audio::Init fallo");
        return 1;
    }

    // Inicializar el sistema de comandos del menu
    InitMenuCommands(*nds);

    Platform::Log(Platform::LogLevel::Info, "Todo inicializado. Corriendo...");

    // 8. Loop principal
    bool quit = false;
    u64 last_log = Platform::GetMSCount();
    u64 last_pc_log = last_log;
    u32 frame_count = 0;
    int dump_count = 0;
    u32 last_ticks = SDL_GetTicks();

    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            input.ProcessEvent(ev, quit);
        }

        if (Platform::ShouldStop()) {
            Platform::Log(Platform::LogLevel::Info, "El core pidio parar.");
            break;
        }

        nds->SetKeyMask(input.GetKeyMask());
        if (input.GetKeyMask() != 0) { printf("[INPUT] mask=0x%08X\n", input.GetKeyMask()); }
        if (input.IsTouchActive()) {
            nds->TouchScreen((u16)input.GetTouchX(), (u16)input.GetTouchY());
        }

        u32 total_lines = 0;
        while (total_lines < 263) {
            total_lines += nds->RunFrame();
        }

        // Chequear comandos del menu cada 30 frames (~0.5s a 60fps)
        static int cmd_counter = 0;
        if (++cmd_counter >= 30) {
            cmd_counter = 0;
            CheckMenuCommands(*nds, video, quit);
            WriteMenuState(*nds);
        }

        void* top_fb = nullptr;
        void* bottom_fb = nullptr;
        bool is_ram = nds->GPU.GetFramebuffers(&top_fb, &bottom_fb);
        if (is_ram) {
            // Software renderer: punteros a RAM
            if ((dump_count % 300 == 0) && top_fb && bottom_fb) {
                FILE* f = fopen("/tmp/frame_top.raw", "wb");
                if (f) { fwrite(top_fb, 1, 256*192*4, f); fclose(f); }
                f = fopen("/tmp/frame_bot.raw", "wb");
                if (f) { fwrite(bottom_fb, 1, 256*192*4, f); fclose(f); }
                printf("[DUMP] frame %d\n", dump_count);
                dump_count++;
            }
            video.PresentFrame((const uint32_t*)top_fb, (const uint32_t*)bottom_fb);
        } else {
            // GLRenderer: top_fb apunta a GLuint
            if (top_fb) {
                GLuint tex = *(GLuint*)top_fb;
                video.PresentGLTexture(tex, 0, 0);  // layer 0 = top
                video.SwapEGL(0);
                if (video.IsDualDisplay()) {
                    video.PresentGLTexture(tex, 1, 1);  // layer 1 = bottom
                    video.SwapEGL(1);
                }
            }
        }

        frame_count++;

        u32 now_ticks = SDL_GetTicks();
        u32 elapsed = now_ticks - last_ticks;
        if (elapsed < 16) {
            SDL_Delay(16 - elapsed);
        }
        last_ticks = SDL_GetTicks();

        u64 now = Platform::GetMSCount();
        if (now - last_log > 5000) {
            double fps = frame_count * 1000.0 / (double)(now - last_log);
            Platform::Log(Platform::LogLevel::Info, "FPS: %.2f", fps);
            frame_count = 0;
            last_log = now;
        }

        // Log del estado cada 1 segundo
        if (now - last_pc_log > 1000) {
            u32 pc9 = nds->ARM9.R[15];
            u32 pc7 = nds->ARM7.R[15];
            u32 cpsr9 = nds->ARM9.CPSR;
            u32 ime9 = nds->IME[0];
            u32 ie9 = nds->IE[0];
            u32 if9 = nds->IF[0];
            u32 halted9 = nds->ARM9.Halted;
            u32 powcnt1 = nds->PowerControl9;
            u32 dispcnt_a = nds->GPU.GPU2D_A.DispCnt;
            u32 dispcnt_b = nds->GPU.GPU2D_B.DispCnt;
            u32 fbsum = 0;
            printf("[DS] PC9=0x%08X PC7=0x%08X CPSR9=0x%08X IME=%d IE=0x%08X IF=0x%08X HALT=%d DISPCNT_A=0x%08X POWCNT1=0x%08X VCount=%d\n",
                   pc9, pc7, cpsr9,
                   ime9, ie9, if9, halted9,
                   dispcnt_a, powcnt1,
                   nds->GPU.VCount);
            last_pc_log = now;
        }
    }

    Platform::Log(Platform::LogLevel::Info, "Saliendo...");
    audio.DeInit();
    input.DeInit();
    video.DeInit();
    return 0;
}
