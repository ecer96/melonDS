#ifndef SDL_MIN_INPUT_H
#define SDL_MIN_INPUT_H

#include <SDL2/SDL.h>
#include <cstdint>

namespace melonDS {
namespace FrontendSDL {

enum DSButton {
    DS_A = 0, DS_B = 1, DS_Select = 2, DS_Start = 3,
    DS_Right = 4, DS_Left = 5, DS_Up = 6, DS_Down = 7,
    DS_R = 8, DS_L = 9, DS_X = 10, DS_Y = 11,
};

class Input
{
public:
    Input();
    ~Input();
    bool Init();
    void DeInit();
    bool ProcessEvent(const SDL_Event& ev, bool& quit);
    uint32_t GetKeyMask() const { return key_mask_; }
    bool IsTouchActive() const { return touch_active_; }
    int  GetTouchX() const { return touch_x_; }
    int  GetTouchY() const { return touch_y_; }
    void SetBottomWindowRect(int x, int y, int w, int h) {
        bot_win_x_ = x; bot_win_y_ = y;
        bot_win_w_ = w; bot_win_h_ = h;
    }

private:
    uint32_t key_mask_;
    bool     touch_active_;
    int      touch_x_;
    int      touch_y_;
    int bot_win_x_ = 0;
    int bot_win_y_ = 0;
    int bot_win_w_ = 256;
    int bot_win_h_ = 192;
    static constexpr int MAX_CONTROLLERS = 4;
    SDL_GameController* controllers_[MAX_CONTROLLERS];
    int num_controllers_;

    void SetButton(int ds_button, bool pressed);
    void CloseAllControllers();
};

} // namespace FrontendSDL
} // namespace melonDS

#endif
