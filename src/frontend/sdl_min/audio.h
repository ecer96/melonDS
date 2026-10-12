#ifndef SDL_MIN_AUDIO_H
#define SDL_MIN_AUDIO_H

#include <SDL2/SDL.h>
#include <cstdint>

namespace melonDS {
class NDS;
}

namespace melonDS {
namespace FrontendSDL {

class Audio
{
public:
    Audio();
    ~Audio();
    bool Init(melonDS::NDS& nds);
    void DeInit();
    void Update() {}

private:
    SDL_AudioDeviceID device_;
    bool              initialized_;
    melonDS::NDS*     nds_;
    static void AudioCallback(void* userdata, Uint8* stream, int len);
};

} // namespace FrontendSDL
} // namespace melonDS

#endif
