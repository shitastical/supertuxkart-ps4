// SuperTuxKart PS4: DualShock 4 as SDL keyboard events.
//
// SDL2 on PS4 is built with the null video/event driver.  Irrlicht's SDL
// device still polls SDL_PollEvent(), so a small thread polls ScePad and
// pushes the key events STK already understands.  This keeps the game's
// existing keyboard bindings and makes the menus and driving usable without
// a second input abstraction.
//
// Racing layout (Mario Kart 8 Deluxe style):
//   Cross accelerate, Circle brake/reverse, left stick or D-pad steer
//   R2 fire item, L2 drift, L1 nitro
//   Square look back, Triangle rescue, Options pause
// Menus keep Cross to select, Circle/Options to go back.

#include <SDL.h>

#include "input/input_manager.hpp"
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
    bool mode_known = false;
    bool last_in_game = false;

    // One entry per SDL key this thread can emit. UP and DOWN are the
    // throttle and brake in a race and menu navigation outside one, so each
    // key is driven by exactly one control per mode.
    KeyState previous[] = {
        { SDLK_UP, false },        // 0: Cross accelerate / menu up
        { SDLK_DOWN, false },      // 1: Circle brake / menu down
        { SDLK_LEFT, false },      // 2: steer left
        { SDLK_RIGHT, false },     // 3: steer right
        { SDLK_RETURN, false },    // 4: Cross, menu select
        { SDLK_ESCAPE, false },    // 5: Options pause / Circle menu back
        { SDLK_SPACE, false },     // 6: R2, fire item (STK's PA_FIRE)
        { SDLK_v, false },         // 7: L2, drift (STK's PA_DRIFT)
        { SDLK_b, false },         // 8: Square, look back
        { SDLK_n, false },         // 9: L1, nitro
        { SDLK_BACKSPACE, false }, // 10: Triangle, rescue
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

        const bool in_game = input_manager != nullptr &&
                             input_manager->getMode() == InputManager::INGAME;
        const bool cross = (data.buttons & ORBIS_PAD_BUTTON_CROSS) != 0;
        const bool circle = (data.buttons & ORBIS_PAD_BUTTON_CIRCLE) != 0;
        const bool square = (data.buttons & ORBIS_PAD_BUTTON_SQUARE) != 0;
        const bool triangle = (data.buttons & ORBIS_PAD_BUTTON_TRIANGLE) != 0;
        const bool options = (data.buttons & ORBIS_PAD_BUTTON_OPTIONS) != 0;
        const bool l1 = (data.buttons & ORBIS_PAD_BUTTON_L1) != 0;
        const bool l2 = (data.buttons & ORBIS_PAD_BUTTON_L2) != 0;
        const bool r2 = (data.buttons & ORBIS_PAD_BUTTON_R2) != 0;

        bool held[sizeof(previous) / sizeof(previous[0])] = {};
        // Mario Kart 8 Deluxe layout: Cross is the throttle, Circle the brake,
        // the left stick steers, L2 drifts and R2 uses the item. Pushing the
        // stick forward no longer accelerates; in a menu the same stick and
        // D-pad scroll, so a trigger squeeze never moves a list.
        held[0] = (in_game && cross) ||
                  (!in_game && ((data.buttons & ORBIS_PAD_BUTTON_UP) != 0 ||
                   data.leftStick.y < 128 - kStickDeadzone));
        held[1] = (in_game && circle) ||
                  (!in_game && ((data.buttons & ORBIS_PAD_BUTTON_DOWN) != 0 ||
                   data.leftStick.y > 128 + kStickDeadzone));
        held[2] = (data.buttons & ORBIS_PAD_BUTTON_LEFT) != 0 ||
                  data.leftStick.x < 128 - kStickDeadzone;
        held[3] = (data.buttons & ORBIS_PAD_BUTTON_RIGHT) != 0 ||
                  data.leftStick.x > 128 + kStickDeadzone;

        // In a race: R2 fires, L2 drifts, L1 nitros, Square looks back,
        // Triangle rescues, Options pauses. In the menus: Cross selects and
        // Circle/Options back out.
        held[4] = !in_game && cross;
        held[5] = options || (!in_game && circle);
        held[6] = in_game && r2;
        held[7] = in_game && l2;
        held[8] = in_game && square;
        held[9] = in_game && l1;
        held[10] = in_game && triangle;

        // Adopt the pad state after a dialog, and whenever the game switches
        // between menu and race, without emitting the edges that happened
        // while the other side owned input. Selecting a kart with Cross does
        // not fire an item on the first frame of the race, and a trigger held
        // through the transition starts working without a stray menu edge.
        if (resync || !mode_known || in_game != last_in_game)
        {
            for (unsigned int i = 0; i < sizeof(previous) / sizeof(previous[0]); ++i)
                previous[i].down = held[i];
            cross_suppressed = held[4];
            resync = false;
            mode_known = true;
            last_in_game = in_game;
            SDL_Delay(16);
            continue;
        }

        for (unsigned int i = 0; i < sizeof(previous) / sizeof(previous[0]); ++i)
        {
            if (held[i] == previous[i].down)
                continue;
            previous[i].down = held[i];

            // Cross is Return only in a menu. In a race it is UP (index 0),
            // so it never opens the IME or a dialog.
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
