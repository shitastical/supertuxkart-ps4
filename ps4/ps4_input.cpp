// SuperTuxKart PS4: DualShock 4 as SDL keyboard events.
//
// SDL2 on PS4 is built with the null video/event driver.  Irrlicht's SDL
// device still polls SDL_PollEvent(), so a small thread polls ScePad and
// pushes the key events STK already understands.  This keeps the game's
// existing keyboard bindings and makes the menus and driving usable without
// a second input abstraction.

#include <SDL.h>

#include "ps4_kit_diag.h"

#include <orbis/Pad.h>
#include <orbis/UserService.h>

#include <atomic>
#include <cstdint>
#include <thread>

extern "C" bool ps4_ime_active(void);
extern "C" bool ps4_ime_text_field_active(void);
extern "C" bool ps4_ime_request_open(void);

namespace
{
constexpr uint8_t kStickDeadzone = 45;

std::atomic<bool> started{false};
int32_t pad_handle = -1;

struct KeyState
{
    SDL_Keycode key;
    bool down;
};

void push_key(SDL_Keycode key, bool down)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.timestamp = SDL_GetTicks();
    event.key.windowID = 0;
    event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = 0;
    event.key.keysym.sym = key;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    event.key.keysym.mod = KMOD_NONE;
    SDL_PushEvent(&event);
}

bool open_pad()
{
    int32_t user = -1;
    if (sceUserServiceGetInitialUser(&user) < 0)
        return false;
    pad_handle = scePadOpen(user, 0, 0, nullptr);
    ps4_boot_tracef("port: ScePad open -> handle %d (user %d)", pad_handle, user);
    return pad_handle >= 0;
}

void input_thread()
{
    sceUserServiceInitialize(nullptr);
    scePadInit();
    ps4_boot_trace("port: pad thread started (SceUserService/ScePad initialised)");

    // Cross is suppressed while it opens the IME: the IME itself pushes the
    // committed text and the Return. Set when the pad was paused for a dialog,
    // so presses made inside it do not leak into the menu when it closes.
    bool cross_suppressed = false;
    bool resync = false;

    KeyState previous[] = {
        { SDLK_UP, false },
        { SDLK_DOWN, false },
        { SDLK_LEFT, false },
        { SDLK_RIGHT, false },
        { SDLK_RETURN, false },
        { SDLK_ESCAPE, false },
        { SDLK_SPACE, false },
        { SDLK_LSHIFT, false },
    };

    while (true)
    {
        // The system IME dialog reads the pad itself; forwarding the same
        // presses to STK would also drive the menu behind the keyboard.
        if (ps4_ime_active())
        {
            resync = true;
            SDL_Delay(16);
            continue;
        }

        if (pad_handle < 0 && !open_pad())
        {
            SDL_Delay(1000);
            continue;
        }

        OrbisPadData data = {};
        if (scePadReadState(pad_handle, &data) != 0 || !data.connected)
        {
            SDL_Delay(100);
            continue;
        }

        bool held[8] = {};
        // D-pad and left stick both drive the menu / steering arrows.
        held[0] = (data.buttons & ORBIS_PAD_BUTTON_UP) != 0 ||
                  data.leftStick.y < 128 - kStickDeadzone;
        held[1] = (data.buttons & ORBIS_PAD_BUTTON_DOWN) != 0 ||
                  data.leftStick.y > 128 + kStickDeadzone;
        held[2] = (data.buttons & ORBIS_PAD_BUTTON_LEFT) != 0 ||
                  data.leftStick.x < 128 - kStickDeadzone;
        held[3] = (data.buttons & ORBIS_PAD_BUTTON_RIGHT) != 0 ||
                  data.leftStick.x > 128 + kStickDeadzone;
        // Cross selects, Circle backs out, Square fires, Options pauses.
        held[4] = (data.buttons & ORBIS_PAD_BUTTON_CROSS) != 0;
        held[5] = (data.buttons & ORBIS_PAD_BUTTON_CIRCLE) != 0 ||
                  (data.buttons & ORBIS_PAD_BUTTON_OPTIONS) != 0;
        held[6] = (data.buttons & ORBIS_PAD_BUTTON_SQUARE) != 0;
        held[7] = (data.buttons & ORBIS_PAD_BUTTON_L1) != 0 ||
                  (data.buttons & ORBIS_PAD_BUTTON_R1) != 0;

        // Adopt the pad state after the dialog without emitting the edges that
        // happened while the thread was paused (the system dialog owned them).
        if (resync)
        {
            for (unsigned int i = 0; i < sizeof(previous) / sizeof(previous[0]); ++i)
                previous[i].down = held[i];
            cross_suppressed = held[4];
            resync = false;
            SDL_Delay(16);
            continue;
        }

        for (unsigned int i = 0; i < sizeof(previous) / sizeof(previous[0]); ++i)
        {
            if (held[i] == previous[i].down)
                continue;
            previous[i].down = held[i];

            // Cross is Return everywhere except in a focused edit box, where it
            // deliberately opens the system keyboard.
            if (i == 4)
            {
                if (held[i])
                {
                    if (ps4_ime_text_field_active() && ps4_ime_request_open())
                    {
                        cross_suppressed = true;
                        ps4_boot_trace("port: Cross on a focused text field; IME requested");
                        continue;
                    }
                    cross_suppressed = false;
                    push_key(SDLK_RETURN, true);
                }
                else
                {
                    if (cross_suppressed)
                    {
                        cross_suppressed = false;
                        continue;
                    }
                    push_key(SDLK_RETURN, false);
                }
                continue;
            }

            push_key(previous[i].key, held[i]);
        }

        SDL_Delay(16);
    }
}
} // namespace

extern "C" void ps4_input_start(void)
{
    if (started.exchange(true))
        return;
    ps4_boot_trace("port: starting the input thread");
    std::thread(input_thread).detach();
}
