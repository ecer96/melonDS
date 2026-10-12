/*
    audio.cpp - Salida de audio SDL2 para melonDS
    Parte del fork RG DS Plus de melonDS.
    Licencia: GPLv3
*/

#include "audio.h"
#include "Platform.h"

#include "NDS.h"
#include "SPU.h"

#include <cstring>

namespace melonDS {
namespace FrontendSDL {

Audio::Audio()
    : device_(0), initialized_(false), nds_(nullptr)
{}

Audio::~Audio()
{
    DeInit();
}

bool Audio::Init(melonDS::NDS& nds)
{
    nds_ = &nds;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        Platform::Log(Platform::LogLevel::Error,
            "SDL_InitSubSystem(AUDIO) fallo: %s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 48000;                    // frecuencia típica de SDL
    want.format = AUDIO_S16SYS;           // s16 nativo, matches el SPU
    want.channels = 2;                    // estéreo
    want.samples = 1024;                  // buffer de ~21ms a 48kHz
    want.callback = AudioCallback;
    want.userdata = this;

    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (device_ == 0) {
        Platform::Log(Platform::LogLevel::Error,
            "SDL_OpenAudioDevice fallo: %s", SDL_GetError());
        return false;
    }

    Platform::Log(Platform::LogLevel::Info,
        "Audio SDL abierto: %d Hz, %d canales, %d samples",
        have.freq, have.channels, have.samples);

    // Configurar el SPU del core para que use la frecuencia del device.
    // nds.SPU.SetOutputSampleRate((double)have.freq);
       (void)have;

    // Arrancar la reproducción
    SDL_PauseAudioDevice(device_, 0);

    initialized_ = true;
    return true;
}

void Audio::DeInit()
{
    if (device_) {
        SDL_PauseAudioDevice(device_, 1);
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    if (initialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        initialized_ = false;
    }
}

void Audio::AudioCallback(void* userdata, Uint8* stream, int len)
{
    Audio* self = static_cast<Audio*>(userdata);
    if (!self || !self->nds_) {
        SDL_memset(stream, 0, len);
        return;
    }

    // len está en bytes. Cada sample s16 = 2 bytes. Estéreo = 2 samples por frame.
    // samples = len / (2 bytes * 2 canales)
    int samples_wanted = len / 4;
    if (samples_wanted <= 0) return;

    // El SPU lee 'samples_wanted' samples (frames estéreo).
    // ReadOutput espera un buffer s16 interleaved.
    int read = self->nds_->SPU.ReadOutput((s16*)stream, samples_wanted);

    // Si el SPU no produjo suficientes samples, rellenamos con silencio.
    if (read < samples_wanted) {
        int remaining_bytes = (samples_wanted - read) * 4;
        SDL_memset(stream + read * 4, 0, remaining_bytes);
    }
}

} // namespace FrontendSDL
} // namespace melonDS
