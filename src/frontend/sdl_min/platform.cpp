/*
    platform.cpp - Implementacion minima de Platform.h para frontend SDL2
    Parte del fork RG DS Plus de melonDS.
    Licencia: GPLv3 (igual que melonDS)
*/

#include "Platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

namespace melonDS
{
namespace Platform
{

// ============================================================================
// FileHandle: envolvemos FILE* de libc
// ============================================================================

struct FileHandle
{
    FILE* fp;
};

// ============================================================================
// SignalStop: el core nos avisa que pare
// ============================================================================

static std::atomic<bool> g_stopRequested{false};

void SignalStop(StopReason reason, void* userdata)
{
    (void)reason;
    (void)userdata;
    g_stopRequested.store(true);
}

bool ShouldStop()
{
    return g_stopRequested.load();
}

// ============================================================================
// Paths y archivos
// ============================================================================

// Directorio de configuracion: ~/.config/melonDS-sdl/
static std::string GetConfigDir()
{
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) home = ".";
    return std::string(home) + "/.config/melonDS-sdl";
}

std::string GetLocalFilePath(const std::string& filename)
{
    return GetConfigDir() + "/" + filename;
}

FileHandle* OpenFile(const std::string& path, FileMode mode)
{
    const char* mode_str = "rb";

    if ((mode & Write) && (mode & Read)) {
        mode_str = (mode & Preserve) ? "r+b" : "w+b";
    } else if (mode & Write) {
        mode_str = (mode & Append) ? "ab" : "wb";
    } else if (mode & Append) {
        mode_str = "ab";
    } else if (mode & Read) {
        mode_str = "rb";
    }

    FILE* fp = fopen(path.c_str(), mode_str);
    if (!fp) return nullptr;

    FileHandle* fh = new FileHandle();
    fh->fp = fp;
    return fh;
}

FileHandle* OpenLocalFile(const std::string& path, FileMode mode)
{
    // Primero intentamos en el directorio de config
    std::string fullpath = GetConfigDir() + "/" + path;

    // Si es modo escritura, aseguramos que el directorio exista
    if (mode & Write) {
        std::string dir = GetConfigDir();
        mkdir(dir.c_str(), 0755);
        mkdir((dir + "/saves").c_str(), 0755);
        mkdir((dir + "/states").c_str(), 0755);
        mkdir((dir + "/cheats").c_str(), 0755);
    }

    return OpenFile(fullpath, mode);
}

bool FileExists(const std::string& name)
{
    struct stat st;
    return stat(name.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool LocalFileExists(const std::string& name)
{
    std::string fullpath = GetConfigDir() + "/" + name;
    return FileExists(fullpath);
}

bool CheckFileWritable(const std::string& filepath)
{
    // Peligroso: crea el archivo si no existe, como dice el header
    FILE* fp = fopen(filepath.c_str(), "ab");
    if (!fp) return false;
    fclose(fp);
    return true;
}

bool CheckLocalFileWritable(const std::string& filepath)
{
    std::string fullpath = GetConfigDir() + "/" + filepath;
    return CheckFileWritable(fullpath);
}

bool CloseFile(FileHandle* file)
{
    if (!file) return false;
    int r = fclose(file->fp);
    delete file;
    return r == 0;
}

bool IsEndOfFile(FileHandle* file)
{
    if (!file) return true;
    return feof(file->fp) != 0;
}

bool FileReadLine(char* str, int count, FileHandle* file)
{
    if (!file) return false;
    return fgets(str, count, file->fp) != nullptr;
}

u64 FilePosition(FileHandle* file)
{
    if (!file) return 0;
    long pos = ftell(file->fp);
    return pos < 0 ? 0 : (u64)pos;
}

bool FileSeek(FileHandle* file, s64 offset, FileSeekOrigin origin)
{
    if (!file) return false;
    int whence = SEEK_SET;
    switch (origin) {
        case FileSeekOrigin::Start:   whence = SEEK_SET; break;
        case FileSeekOrigin::Current: whence = SEEK_CUR; break;
        case FileSeekOrigin::End:     whence = SEEK_END; break;
    }
    return fseeko(file->fp, (off_t)offset, whence) == 0;
}

void FileRewind(FileHandle* file)
{
    if (file) rewind(file->fp);
}

u64 FileRead(void* data, u64 size, u64 count, FileHandle* file)
{
    if (!file) return 0;
    return (u64)fread(data, (size_t)size, (size_t)count, file->fp);
}

bool FileFlush(FileHandle* file)
{
    if (!file) return false;
    return fflush(file->fp) == 0;
}

u64 FileWrite(const void* data, u64 size, u64 count, FileHandle* file)
{
    if (!file) return 0;
    return (u64)fwrite(data, (size_t)size, (size_t)count, file->fp);
}

u64 FileWriteFormatted(FileHandle* file, const char* fmt, ...)
{
    if (!file) return 0;
    va_list args;
    va_start(args, fmt);
    int r = vfprintf(file->fp, fmt, args);
    va_end(args);
    return r < 0 ? 0 : (u64)r;
}

u64 FileLength(FileHandle* file)
{
    if (!file) return 0;
    long cur = ftell(file->fp);
    if (cur < 0) return 0;
    fseeko(file->fp, 0, SEEK_END);
    long len = ftell(file->fp);
    fseeko(file->fp, cur, SEEK_SET);
    return len < 0 ? 0 : (u64)len;
}

// ============================================================================
// Log
// ============================================================================

void Log(LogLevel level, const char* fmt, ...)
{
    const char* prefix;
    switch (level) {
        case LogLevel::Debug: prefix = "[DEBUG] "; break;
        case LogLevel::Info:  prefix = "[INFO]  "; break;
        case LogLevel::Warn:  prefix = "[WARN]  "; break;
        case LogLevel::Error: prefix = "[ERROR] "; break;
        default:              prefix = "[?]     "; break;
    }
    fprintf(stderr, "%s", prefix);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

// ============================================================================
// Threads
// ============================================================================

struct Thread
{
    std::thread th;
};

Thread* Thread_Create(std::function<void()> func)
{
    Thread* t = new Thread();
    t->th = std::thread(func);
    return t;
}

void Thread_Free(Thread* thread)
{
    if (!thread) return;
    if (thread->th.joinable()) thread->th.detach();
    delete thread;
}

void Thread_Wait(Thread* thread)
{
    if (!thread) return;
    if (thread->th.joinable()) thread->th.join();
}

// ============================================================================
// Semaphore
// ============================================================================

struct Semaphore
{
    std::mutex mtx;
    std::condition_variable cv;
    int count;
    Semaphore() : count(0) {}
};

Semaphore* Semaphore_Create()
{
    return new Semaphore();
}

void Semaphore_Free(Semaphore* sema)
{
    delete sema;
}

void Semaphore_Reset(Semaphore* sema)
{
    if (!sema) return;
    std::lock_guard<std::mutex> lock(sema->mtx);
    sema->count = 0;
}

void Semaphore_Wait(Semaphore* sema)
{
    if (!sema) return;
    std::unique_lock<std::mutex> lock(sema->mtx);
    sema->cv.wait(lock, [sema]{ return sema->count > 0; });
    sema->count--;
}

bool Semaphore_TryWait(Semaphore* sema, int timeout_ms)
{
    if (!sema) return false;
    std::unique_lock<std::mutex> lock(sema->mtx);
    if (timeout_ms == 0) {
        if (sema->count > 0) { sema->count--; return true; }
        return false;
    }
    bool ok = sema->cv.wait_for(lock,
        std::chrono::milliseconds(timeout_ms),
        [sema]{ return sema->count > 0; });
    if (ok) sema->count--;
    return ok;
}

void Semaphore_Post(Semaphore* sema, int count)
{
    if (!sema) return;
    std::lock_guard<std::mutex> lock(sema->mtx);
    sema->count += count;
    sema->cv.notify_all();
}

// ============================================================================
// Mutex
// ============================================================================

struct Mutex
{
    std::mutex mtx;
};

Mutex* Mutex_Create() { return new Mutex(); }
void Mutex_Free(Mutex* mutex) { delete mutex; }
void Mutex_Lock(Mutex* mutex) { if (mutex) mutex->mtx.lock(); }
void Mutex_Unlock(Mutex* mutex) { if (mutex) mutex->mtx.unlock(); }
bool Mutex_TryLock(Mutex* mutex) { return mutex ? mutex->mtx.try_lock() : false; }

// ============================================================================
// Timing
// ============================================================================

void Sleep(u64 usecs)
{
    std::this_thread::sleep_for(std::chrono::microseconds(usecs));
}

u64 GetMSCount()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return (u64)std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

u64 GetUSCount()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return (u64)std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

// ============================================================================
// Save files (por ahora, no-op; el frontend maneja esto)
// ============================================================================

void WriteNDSSave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
    (void)savedata; (void)savelen; (void)writeoffset; (void)writelen; (void)userdata;
    // TODO: implementar en el frontend SDL
}

void WriteGBASave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
    (void)savedata; (void)savelen; (void)writeoffset; (void)writelen; (void)userdata;
}

void WriteFirmware(const Firmware& firmware, u32 writeoffset, u32 writelen, void* userdata)
{
    (void)firmware; (void)writeoffset; (void)writelen; (void)userdata;
}

void WriteDateTime(int year, int month, int day, int hour, int minute, int second, void* userdata)
{
    (void)year; (void)month; (void)day; (void)hour; (void)minute; (void)second; (void)userdata;
}

// ============================================================================
// Multiplayer (no soportado en el frontend minimo)
// ============================================================================

void MP_Begin(void*) {}
void MP_End(void*) {}
int MP_SendPacket(u8*, int, u64, void*) { return 0; }
int MP_RecvPacket(u8*, u64*, void*) { return 0; }
int MP_SendCmd(u8*, int, u64, void*) { return 0; }
int MP_SendReply(u8*, int, u64, u16, void*) { return 0; }
int MP_SendAck(u8*, int, u64, void*) { return 0; }
int MP_RecvHostPacket(u8*, u64*, void*) { return 0; }
u16 MP_RecvReplies(u8*, u64, u16, void*) { return 0; }

// ============================================================================
// Network (no soportado)
// ============================================================================

int Net_SendPacket(u8*, int, void*) { return 0; }
int Net_RecvPacket(u8*, void*) { return 0; }

// ============================================================================
// Camera (no soportado)
// ============================================================================

void Camera_Start(int, void*) {}
void Camera_Stop(int, void*) {}
void Camera_CaptureFrame(int, u32*, int, int, bool, void*) {}

// ============================================================================
// Microphone (no soportado)
// ============================================================================

void Mic_Start(void*) {}
void Mic_Stop(void*) {}
int Mic_ReadInput(s16*, int, void*) { return 0; }

// ============================================================================
// AAC (no soportado)
// ============================================================================

struct AACDecoder {};

AACDecoder* AAC_Init() { return nullptr; }
void AAC_DeInit(AACDecoder*) {}
bool AAC_Configure(AACDecoder*, int, int) { return false; }
bool AAC_DecodeFrame(AACDecoder*, const void*, int, void*, int) { return false; }

// ============================================================================
// Addons (no soportados)
// ============================================================================

bool Addon_KeyDown(KeyType, void*) { return false; }
void Addon_RumbleStart(u32, void*) {}
void Addon_RumbleStop(void*) {}
float Addon_MotionQuery(MotionQueryType, void*) { return 0.0f; }

// ============================================================================
// DynamicLibrary (wrapper de dlopen)
// ============================================================================

struct DynamicLibrary
{
    void* handle;
};

DynamicLibrary* DynamicLibrary_Load(const char* lib)
{
    void* h = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (!h) return nullptr;
    DynamicLibrary* dl = new DynamicLibrary();
    dl->handle = h;
    return dl;
}

void DynamicLibrary_Unload(DynamicLibrary* lib)
{
    if (!lib) return;
    if (lib->handle) dlclose(lib->handle);
    delete lib;
}

void* DynamicLibrary_LoadFunction(DynamicLibrary* lib, const char* name)
{
    if (!lib || !lib->handle) return nullptr;
    return dlsym(lib->handle, name);
}

} // namespace Platform
} // namespace melonDS