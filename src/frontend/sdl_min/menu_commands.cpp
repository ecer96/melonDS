/*
    menu_commands.cpp - Lector de comandos del menu custom
    Parte del fork RG DS Plus de melonDS.
    Licencia: GPLv3
*/

#include "menu_commands.h"
#include "NDS.h"
#include "GPU.h"
#include "video.h"
#include "Platform.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

namespace melonDS {
namespace FrontendSDL {

static const char* CMD_PATH   = "/tmp/melonds-cmd";
static const char* STATE_TMP  = "/tmp/melonds-state.tmp";
static const char* STATE_PATH = "/tmp/melonds-state";

static int  g_hires     = 1;
static bool g_fast_fwd  = false;
static int  g_save_slot = 0;

static constexpr u32 SAVESTATE_SIZE = 32 * 1024 * 1024;
static u8* g_savestate_buf = nullptr;

void InitMenuCommands(NDS& nds)
{
    (void)nds;
    if (!g_savestate_buf) {
        g_savestate_buf = (u8*)malloc(SAVESTATE_SIZE);
    }
    unlink(CMD_PATH);
    unlink(STATE_PATH);
}

static bool DoSaveState(NDS& nds, int slot)
{
    if (!g_savestate_buf) return false;

    memset(g_savestate_buf, 0, SAVESTATE_SIZE);
    Savestate state(g_savestate_buf, SAVESTATE_SIZE, true);
    if (!nds.DoSavestate(&state)) return false;
    if (state.Error) return false;

    u32 real_len = state.Length();

    char path[64];
    snprintf(path, sizeof(path), "/tmp/melonds-save-%d", slot);
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(g_savestate_buf, 1, real_len, f);
    fclose(f);
    return true;
}

static bool DoLoadState(NDS& nds, int slot)
{
    if (!g_savestate_buf) return false;

    char path[64];
    snprintf(path, sizeof(path), "/tmp/melonds-save-%d", slot);
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > (long)SAVESTATE_SIZE) {
        fclose(f);
        return false;
    }

    size_t len = fread(g_savestate_buf, 1, (size_t)file_size, f);
    fclose(f);
    if (len != (size_t)file_size) return false;

    Savestate state(g_savestate_buf, (u32)file_size, false);
    if (!nds.DoSavestate(&state)) return false;
    return !state.Error;
}

static void ToggleFastForward(NDS& nds)
{
    (void)nds;
    g_fast_fwd = !g_fast_fwd;
}

static void SetHires(NDS& nds, Video& video, int mode)
{
    (void)nds;
    g_hires = mode;
    video.SetScaleMode(mode);
}

bool CheckMenuCommands(NDS& nds, Video& video, bool& quit)
{
    FILE* f = fopen(CMD_PATH, "r");
    if (!f) return false;

    char cmd[128] = {0};
    if (!fgets(cmd, sizeof(cmd), f)) {
        fclose(f);
        return false;
    }
    fclose(f);

    cmd[strcspn(cmd, "\r\n")] = 0;

    if (strcmp(cmd, "save") == 0) {
        DoSaveState(nds, g_save_slot);
    } else if (strcmp(cmd, "load") == 0) {
        DoLoadState(nds, g_save_slot);
    } else if (strcmp(cmd, "ff") == 0) {
        ToggleFastForward(nds);
    } else if (strncmp(cmd, "hires=", 6) == 0) {
        int scale = atoi(cmd + 6);
        if (scale >= 0 && scale <= 2) SetHires(nds, video, scale);
    } else if (strncmp(cmd, "shader=", 7) == 0) {
        // TODO
    } else if (strcmp(cmd, "restart") == 0) {
        // TODO
    } else if (strcmp(cmd, "quit") == 0) {
        quit = true;
        unlink(CMD_PATH);
        return true;
    }

    unlink(CMD_PATH);
    return false;
}

void WriteMenuState(NDS& nds)
{
    (void)nds;
    FILE* f = fopen(STATE_TMP, "w");
    if (!f) return;

    fprintf(f, "hires=%d\n", g_hires);
    fprintf(f, "ff=%d\n", g_fast_fwd ? 1 : 0);
    fprintf(f, "save_slot=%d\n", g_save_slot);
    fprintf(f, "shader=none\n");
    fprintf(f, "volume=50\n");

    fclose(f);
    rename(STATE_TMP, STATE_PATH);
}

} // namespace FrontendSDL
} // namespace melonDS
