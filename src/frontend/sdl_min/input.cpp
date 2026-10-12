/*
    input.cpp - Mapeo de botones y touchscreen a melonDS
    Parte del fork RG DS Plus de melonDS.
    Licencia: GPLv3
*/

#include "input.h"
#include "Platform.h"

#include <cstring>

namespace melonDS {
namespace FrontendSDL {

Input::Input()
    : key_mask_(0xFFF), touch_active_(false), touch_x_(0), touch_y_(0),
      num_controllers_(0)
{
    memset(controllers_, 0, sizeof(controllers_));
}

Input::~Input()
{
    DeInit();
}

bool Input::Init()
{
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        Platform::Log(Platform::LogLevel::Warn,
            "SDL_InitSubSystem(GAMECONTROLLER) fallo: %s", SDL_GetError());
        // No es fatal: podemos jugar con teclado.
        return true;
    }

    // Abrir todos los controllers conectados
    int n = SDL_NumJoysticks();
    Platform::Log(Platform::LogLevel::Info,
        "Joysticks detectados: %d", n);

    for (int i = 0; i < n && num_controllers_ < MAX_CONTROLLERS; i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* gc = SDL_GameControllerOpen(i);
            if (gc) {
                controllers_[num_controllers_++] = gc;
                const char* name = SDL_GameControllerName(gc);
                Platform::Log(Platform::LogLevel::Info,
                    "GameController abierto: %s", name ? name : "(sin nombre)");
            }
        }
    }

    return true;
}

void Input::DeInit()
{
    CloseAllControllers();
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
}

void Input::CloseAllControllers()
{
    for (int i = 0; i < num_controllers_; i++) {
        if (controllers_[i]) {
            SDL_GameControllerClose(controllers_[i]);
            controllers_[i] = nullptr;
        }
    }
    num_controllers_ = 0;
}

void Input::SetButton(int ds_button, bool pressed)
{
    uint32_t bit = 1u << ds_button;
    if (pressed) {
        key_mask_ &= ~bit;
    } else {
        key_mask_ |= bit;
    }
}

bool Input::ProcessEvent(const SDL_Event& ev, bool& quit)
{
    switch (ev.type) {
    case SDL_QUIT:
        quit = true;
        return true;

    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        bool pressed = (ev.type == SDL_KEYDOWN);
        if (ev.key.repeat) return true;
        printf("[KEY] sym=0x%08X pressed=%d\n", ev.key.keysym.sym, pressed);
        fflush(stdout);

        switch (ev.key.keysym.sym) {
        // D-pad
        case SDLK_UP:      SetButton(DS_Up,     pressed); return true;
        case SDLK_DOWN:    SetButton(DS_Down,   pressed); return true;
        case SDLK_LEFT:    SetButton(DS_Left,   pressed); return true;
        case SDLK_RIGHT:   SetButton(DS_Right,  pressed); return true;

        // Botones de accion
        case SDLK_z:       SetButton(DS_A,      pressed); return true;
        case SDLK_x:       SetButton(DS_B,      pressed); return true;
        case SDLK_a:       SetButton(DS_Y,      pressed); return true;
        case SDLK_s:       SetButton(DS_X,      pressed); return true;

        // Shoulder
        case SDLK_q:       SetButton(DS_L,      pressed); return true;
        case SDLK_w:       SetButton(DS_R,      pressed); return true;

        // Start/Select
        case SDLK_RETURN:  SetButton(DS_Start,  pressed); return true;
        case SDLK_BACKSPACE: SetButton(DS_Select, pressed); return true;

        // Salir
        case SDLK_ESCAPE:  quit = true; return true;

        default: return false;
        }
    }

    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        bool pressed = (ev.type == SDL_CONTROLLERBUTTONDOWN);
        switch (ev.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    SetButton(DS_Up,    pressed); return true;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  SetButton(DS_Down,  pressed); return true;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  SetButton(DS_Left,  pressed); return true;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: SetButton(DS_Right, pressed); return true;

        case SDL_CONTROLLER_BUTTON_A:          SetButton(DS_A,     pressed); return true;
        case SDL_CONTROLLER_BUTTON_B:          SetButton(DS_B,     pressed); return true;
        case SDL_CONTROLLER_BUTTON_X:          SetButton(DS_X,     pressed); return true;
        case SDL_CONTROLLER_BUTTON_Y:          SetButton(DS_Y,     pressed); return true;

        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  SetButton(DS_L, pressed); return true;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: SetButton(DS_R, pressed); return true;

        case SDL_CONTROLLER_BUTTON_START:      SetButton(DS_Start,  pressed); return true;
        case SDL_CONTROLLER_BUTTON_BACK:       SetButton(DS_Select, pressed); return true;

        default: return false;
        }
    }

    // Touchscreen en la ventana inferior
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        if (ev.button.button != SDL_BUTTON_LEFT) return false;
        if (ev.type == SDL_MOUSEBUTTONDOWN) {
            touch_active_ = true;
        } else {
            touch_active_ = false;
        }
        // Mapear coords de ventana a coords del DS
        int rx = ev.button.x - bot_win_x_;
        int ry = ev.button.y - bot_win_y_;
        if (rx < 0) rx = 0;
        if (ry < 0) ry = 0;
        if (rx >= bot_win_w_) rx = bot_win_w_ - 1;
        if (ry >= bot_win_h_) ry = bot_win_h_ - 1;
        touch_x_ = (rx * 256) / (bot_win_w_ > 0 ? bot_win_w_ : 1);
        touch_y_ = (ry * 192) / (bot_win_h_ > 0 ? bot_win_h_ : 1);
        return true;
    }

    case SDL_MOUSEMOTION: {
        if (touch_active_) {
            int rx = ev.motion.x - bot_win_x_;
            int ry = ev.motion.y - bot_win_y_;
            if (rx < 0) rx = 0;
            if (ry < 0) ry = 0;
            if (rx >= bot_win_w_) rx = bot_win_w_ - 1;
            if (ry >= bot_win_h_) ry = bot_win_h_ - 1;
            touch_x_ = (rx * 256) / (bot_win_w_ > 0 ? bot_win_w_ : 1);
            touch_y_ = (ry * 192) / (bot_win_h_ > 0 ? bot_win_h_ : 1);
            return true;
        }
        return false;
    }

    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION: {
        // Touch nativo (tablets, handhelds con pantalla tactil)
        bool active = (ev.type != SDL_FINGERUP);
        touch_active_ = active;
        // ev.tfinger.x/y estan normalizados [0,1]
        touch_x_ = (int)(ev.tfinger.x * 255);
        touch_y_ = (int)(ev.tfinger.y * 191);
        if (touch_x_ < 0) touch_x_ = 0;
        if (touch_y_ < 0) touch_y_ = 0;
        if (touch_x_ > 255) touch_x_ = 255;
        if (touch_y_ > 191) touch_y_ = 191;
        return true;
    }

    default:
        return false;
    }
}

} // namespace FrontendSDL
} // namespace melonDS