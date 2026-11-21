// SuperTuxKart PS4: DualShock 4 controllers as STK gamepad devices.
//
// SDL2 on PS4 is built with the null video/event driver, so there is no SDL
// controller hotplug.  ScePad gives every signed-in user one standard pad at
// index 0; a small thread opens one handle per user and queues the same events
// the SDL controller backend would send.  STK sees real gamepads instead of
// one keyboard, so a second controller is a distinct input device and can join
// a split-screen race.  The stick is analog in a race; in menus it is treated
// as a full direction so navigation stays quick.
//
// Racing layout (console racing style):
//   Cross accelerate, Circle brake/reverse, left stick or D-pad steer
//   R2 fire item, L2 drift, L1 nitro
//   Square look back, Triangle rescue, Options pause
// Menus keep Cross to select, Circle/Options to go back.

#include <SDL.h>

#include "input/device_manager.hpp"
#include "input/gamepad_config.hpp"
#include "input/gamepad_device.hpp"
#include "input/input_manager.hpp"
#include "ps4_kit_diag.h"

#include <orbis/Pad.h>
#include <orbis/UserService.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" bool ps4_ime_active(void);
extern "C" bool ps4_ime_text_field_active(void);
extern "C" bool ps4_ime_request_open(void);

namespace
{
constexpr int kPadCount = 4;
constexpr int kIrrBase = 0x4000;
constexpr int kButtonCount = 16;
constexpr int kStickDeadzone = 30;

enum PadButton
{
    PS4_CROSS = 0,
    PS4_CIRCLE,
    PS4_SQUARE,
    PS4_TRIANGLE,
    PS4_L1,
    PS4_L2,
    PS4_R2,
    PS4_MENU_BACK,
    PS4_OPTIONS,
    PS4_BUTTON_COUNT
};

struct PadEvent
{
    Input::InputType type;
    int device;
    int button;
    Input::AxisDirection direction;
    int value;
};

struct PadState
{
    std::atomic<int32_t> handle{-1};
    // Set only after scePadReadState returns a connected pad.  Registration
    // waits for this, so an empty slot is never announced as a controller.
    std::atomic<bool> read_ok{false};

    // Main thread only.
    bool registered = false;
    GamePadDevice* device = nullptr;

    // Only the input thread touches these.
    int32_t user = -1;
    bool have_previous = false;
    bool previous_buttons[PS4_BUTTON_COUNT] = {};
    int previous_x = 0;
    int previous_y = 0;
    bool cross_suppressed = false;
    uint32_t next_open_ms = 0;
};

PadState pads[kPadCount];
std::mutex queue_mutex;
std::deque<PadEvent> queue;
std::atomic<bool> started{false};

int pad_device_id(int index)
{
    return kIrrBase + index;
}   // pad_device_id

void queue_event(Input::InputType type, int device, int button,
                 Input::AxisDirection direction, int value)
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    // A stuck main thread must not let the pad thread grow the queue forever.
    if (queue.size() >= 1024)
        queue.pop_front();
    queue.push_back({type, device, button, direction, value});
}   // queue_event

void queue_button(int index, int button, bool down)
{
    queue_event(Input::IT_STICKBUTTON, pad_device_id(index), button,
                Input::AD_NEUTRAL, down ? Input::MAX_VALUE : 0);
}   // queue_button

void queue_axis(int index, int axis, int value)
{
    queue_event(Input::IT_STICKMOTION, pad_device_id(index), axis,
                Input::AD_NEUTRAL, value);
}   // queue_axis

void release_pad(int index)
{
    PadState& pad = pads[index];
    if (!pad.have_previous)
    {
        pad.cross_suppressed = false;
        return;
    }

    for (int button = 0; button < PS4_BUTTON_COUNT; ++button)
    {
        if (pad.previous_buttons[button])
            queue_button(index, button, false);
        pad.previous_buttons[button] = false;
    }
    if (pad.previous_x != 0)
        queue_axis(index, 0, 0);
    if (pad.previous_y != 0)
        queue_axis(index, 1, 0);

    pad.previous_x = 0;
    pad.previous_y = 0;
    pad.have_previous = false;
    pad.cross_suppressed = false;
}   // release_pad

void add_login_user(std::vector<int32_t>& users, int32_t user)
{
    if (user < 0 || user == ORBIS_USER_SERVICE_USER_ID_SYSTEM)
        return;
    if (std::find(users.begin(), users.end(), user) == users.end())
        users.push_back(user);
}   // add_login_user

std::vector<int32_t> login_users()
{
    std::vector<int32_t> users;

    int32_t initial_user = -1;
    const int32_t initial_result =
        sceUserServiceGetInitialUser(&initial_user);
    if (initial_result >= 0)
        add_login_user(users, initial_user);

    // Every signed-in user owns one standard pad at index 0.  The second
    // player's controller is NOT at index 1 of the first user, so the opener
    // must never invent extra indices to fill a slot.
    OrbisUserServiceLoginUserIdList user_list = {};
    const int32_t list_result =
        sceUserServiceGetLoginUserIdList(&user_list);
    if (list_result >= 0)
    {
        for (int i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; ++i)
            add_login_user(users, user_list.userId[i]);
    }

    int32_t foreground_result = 0;
    if (users.empty())
    {
        int32_t foreground_user = -1;
        foreground_result = sceUserServiceGetForegroundUser(&foreground_user);
        if (foreground_result >= 0)
            add_login_user(users, foreground_user);
    }

    // The thread retries every second; report only changes (and failures),
    // so boot-trace.log shows when the second user signs in.
    static std::vector<int32_t> traced_users;
    static int32_t traced_results[3] = {-2, -2, -2};
    const int32_t results[3] = {initial_result, list_result,
                                foreground_result};
    if (users != traced_users ||
        results[0] != traced_results[0] ||
        results[1] != traced_results[1] ||
        results[2] != traced_results[2])
    {
        std::string list;
        for (int32_t user : users)
        {
            if (!list.empty())
                list += ", ";
            list += std::to_string(user);
        }
        ps4_boot_tracef(
            "port: login users (%zu): %s (initial 0x%08x list 0x%08x "
            "foreground 0x%08x)",
            users.size(), list.empty() ? "none" : list.c_str(),
            static_cast<uint32_t>(initial_result),
            static_cast<uint32_t>(list_result),
            static_cast<uint32_t>(foreground_result));
        traced_users = users;
        for (int i = 0; i < 3; ++i)
            traced_results[i] = results[i];
    }

    return users;
}   // login_users

struct PadCandidate
{
    int32_t user;
};

std::vector<PadCandidate> pad_candidates(const std::vector<int32_t>& users)
{
    std::vector<PadCandidate> candidates;

    // ScePad routes one standard controller per signed-in user at index 0.
    // The index argument is not a "second controller" selector.  Probing
    // indices 1-3 can hand back a handle that never reports a physical pad;
    // that empty slot then blocks the second user's real pad when it appears.
    for (int32_t user : users)
        candidates.push_back({user});
    return candidates;
}   // pad_candidates

bool candidate_in_use(int slot, int32_t user)
{
    for (int i = 0; i < kPadCount; ++i)
    {
        if (i == slot || pads[i].handle.load() < 0)
            continue;
        if (pads[i].user == user)
            return true;
    }
    return false;
}   // candidate_in_use

bool handle_in_use(int slot, int32_t handle)
{
    for (int i = 0; i < kPadCount; ++i)
    {
        if (i != slot && pads[i].handle.load() == handle)
            return true;
    }
    return false;
}   // handle_in_use

void trace_open_failure(int slot, int32_t user, int32_t result)
{
    // The input thread retries every second; report each distinct failure
    // once instead of filling the boot trace.
    char key[64];
    std::snprintf(key, sizeof key, "%d/%d/%08x", slot, user,
                  static_cast<uint32_t>(result));
    static std::vector<std::string> reported;
    if (std::find(reported.begin(), reported.end(), key) != reported.end())
        return;
    reported.push_back(key);
    ps4_boot_tracef("port: slot %d: scePadOpen user %d failed 0x%08x",
                    slot, user, static_cast<uint32_t>(result));
}   // trace_open_failure

void trace_read_failure(int slot, int32_t handle, int32_t result,
                        bool connected)
{
    char key[80];
    std::snprintf(key, sizeof key, "%d/%d/%08x/%d", slot, handle,
                  static_cast<uint32_t>(result), connected ? 1 : 0);
    static std::vector<std::string> reported;
    if (std::find(reported.begin(), reported.end(), key) != reported.end())
        return;
    reported.push_back(key);
    ps4_boot_tracef(
        "port: slot %d: scePadReadState handle %d failed 0x%08x "
        "connected %d",
        slot, handle, static_cast<uint32_t>(result), connected ? 1 : 0);
}   // trace_read_failure

bool open_pad(int slot, uint32_t now, const std::vector<int32_t>& users)
{
    PadState& pad = pads[slot];
    if (pad.handle.load() >= 0)
        return true;
    if (now < pad.next_open_ms)
        return false;
    pad.next_open_ms = now + 1000;

    const std::vector<PadCandidate> candidates = pad_candidates(users);
    for (const PadCandidate& candidate : candidates)
    {
        if (candidate_in_use(slot, candidate.user))
            continue;

        // Standard pad, index 0, for this signed-in user.  Do not probe other
        // indices: a non-reporting handle would take this slot before the
        // second user signs in.
        int32_t handle = scePadOpen(candidate.user, 0, 0, nullptr);
        bool adopted = false;
        if (static_cast<uint32_t>(handle) == ORBIS_PAD_ERROR_ALREADY_OPENED)
        {
            // Another part of the app may already own this pad.  Use its
            // handle instead of failing forever.
            handle = scePadGetHandle(candidate.user, 0, 0);
            adopted = handle >= 0;
        }
        if (handle < 0)
        {
            trace_open_failure(slot, candidate.user, handle);
            continue;
        }
        if (handle_in_use(slot, handle))
        {
            // Do not attach the same physical pad to two players.
            if (!adopted)
                scePadClose(handle);
            continue;
        }

        pad.handle.store(handle);
        pad.user = candidate.user;
        pad.read_ok.store(false);
        pad.have_previous = false;
        pad.cross_suppressed = false;
        ps4_boot_tracef(
            "port: slot %d: opened ScePad user %d handle %d "
            "(waiting for connection)",
            slot, candidate.user, handle);
        return true;
    }

    return false;
}   // open_pad

void disconnect_pad(int index)
{
    PadState& pad = pads[index];
    release_pad(index);
    const int32_t handle = pad.handle.exchange(-1);
    pad.read_ok.store(false);
    pad.user = -1;
    pad.next_open_ms = SDL_GetTicks() + 1000;
    if (handle >= 0)
        scePadClose(handle);
}   // disconnect_pad

void configure_gamepad(GamepadConfig* config, const std::string& name)
{
    config->setName(name);
    config->setConfigName(irr::core::stringw(name.c_str()));
    config->setEnabled(true);
    config->setNumberOfAxis(2);
    config->setNumberOfButtons(kButtonCount);
    config->setForceFeedback(false);

    // Start from the stock bindings, then replace every action.  These pads
    // are the PS4 port's own devices; there is no SDL mapping string.
    config->setDefaultBinds();
    config->setBinding(PA_ACCEL,       Input::IT_STICKBUTTON, PS4_CROSS);
    config->setBinding(PA_BRAKE,       Input::IT_STICKBUTTON, PS4_CIRCLE);
    config->setBinding(PA_FIRE,        Input::IT_STICKBUTTON, PS4_R2);
    config->setBinding(PA_DRIFT,       Input::IT_STICKBUTTON, PS4_L2);
    config->setBinding(PA_NITRO,       Input::IT_STICKBUTTON, PS4_L1);
    config->setBinding(PA_LOOK_BACK,   Input::IT_STICKBUTTON, PS4_SQUARE);
    config->setBinding(PA_RESCUE,      Input::IT_STICKBUTTON, PS4_TRIANGLE);
    config->setBinding(PA_PAUSE_RACE,  Input::IT_STICKBUTTON, PS4_OPTIONS);

    config->setBinding(PA_STEER_LEFT,  Input::IT_STICKMOTION, 0,
                       Input::AD_NEGATIVE);
    config->setBinding(PA_STEER_RIGHT, Input::IT_STICKMOTION, 0,
                       Input::AD_POSITIVE);
    config->setBinding(PA_MENU_UP,     Input::IT_STICKMOTION, 1,
                       Input::AD_NEGATIVE);
    config->setBinding(PA_MENU_DOWN,   Input::IT_STICKMOTION, 1,
                       Input::AD_POSITIVE);
    config->setBinding(PA_MENU_LEFT,   Input::IT_STICKMOTION, 0,
                       Input::AD_NEGATIVE);
    config->setBinding(PA_MENU_RIGHT,  Input::IT_STICKMOTION, 0,
                       Input::AD_POSITIVE);
    config->setBinding(PA_MENU_SELECT, Input::IT_STICKBUTTON, PS4_CROSS);
    // Circle and Options both back out of a menu; the input thread folds them
    // into one logical button so Circle can still be the in-race brake.
    config->setBinding(PA_MENU_CANCEL, Input::IT_STICKBUTTON, PS4_MENU_BACK);
}   // configure_gamepad

void register_pads(InputManager* manager)
{
    DeviceManager* device_manager = manager->getDeviceManager();
    for (int index = 0; index < kPadCount; ++index)
    {
        PadState& pad = pads[index];
        const bool live = pad.read_ok.load();
        if (pad.registered)
        {
            // Keep the device object, but mark a disconnected pad so it is
            // not treated as an active controller until it comes back.
            if (pad.device != nullptr)
                pad.device->setConnected(live);
            continue;
        }
        if (!live)
            continue;

        const std::string name =
            "PS4 Controller " + std::to_string(index + 1);
        GamepadConfig* config = nullptr;
        device_manager->getConfigForGamepad(pad_device_id(index), name,
                                            &config);
        configure_gamepad(config, name);
        config->setPlugged();

        GamePadDevice* device = new GamePadDevice(
            pad_device_id(index), name, 2, kButtonCount, config);
        device->setConnected(true);
        device->setPlayer(nullptr);
        device_manager->addGamepad(device);
        pad.device = device;
        pad.registered = true;
        ps4_boot_tracef("port: registered %s (device id %d)", name.c_str(),
                        pad_device_id(index));
    }
}   // register_pads

int stick_axis(uint8_t raw, bool negative_button, bool positive_button,
               bool in_game)
{
    if (negative_button)
        return -Input::MAX_VALUE;
    if (positive_button)
        return Input::MAX_VALUE;

    int value = (static_cast<int>(raw) - 128) * 256;
    if (std::abs(value) < kStickDeadzone * 256)
        return 0;

    if (!in_game)
        return value < 0 ? -Input::MAX_VALUE : Input::MAX_VALUE;

    if (value > 32767)
        value = 32767;
    else if (value < -32767)
        value = -32767;
    return value;
}   // stick_axis

void input_thread()
{
    const int32_t user_result = sceUserServiceInitialize(nullptr);
    const int32_t pad_result = scePadInit();
    ps4_boot_tracef(
        "port: pad thread started (user service 0x%08x pad 0x%08x build "
        __DATE__ " " __TIME__ ")",
        static_cast<uint32_t>(user_result),
        static_cast<uint32_t>(pad_result));

    bool ime_paused = false;
    bool mode_known = false;
    bool last_in_game = false;

    while (true)
    {
        // The system IME dialog reads the pad itself; forwarding the same
        // presses to STK would also drive the menu behind the keyboard.
        if (ps4_ime_active())
        {
            if (!ime_paused)
            {
                for (int index = 0; index < kPadCount; ++index)
                    release_pad(index);
                ime_paused = true;
            }
            SDL_Delay(16);
            continue;
        }

        if (ime_paused)
        {
            ime_paused = false;
            mode_known = false;
        }

        InputManager* manager = input_manager;
        if (manager == nullptr)
        {
            SDL_Delay(16);
            continue;
        }

        const bool in_game = manager->getMode() == InputManager::INGAME;
        if (!mode_known || in_game != last_in_game)
        {
            // Selecting a kart with Cross must not fire an item or accelerate
            // on the first race frame.  Adopt the current state without edges.
            for (int index = 0; index < kPadCount; ++index)
            {
                pads[index].have_previous = false;
                pads[index].cross_suppressed = false;
            }
            mode_known = true;
            last_in_game = in_game;
        }

        const uint32_t now = SDL_GetTicks();
        const std::vector<int32_t> users = login_users();

        // A signed-out user's handle is no longer usable.  Do not keep it in
        // a slot: the next signed-in user must be able to take that pad.
        if (!users.empty())
        {
            for (int index = 0; index < kPadCount; ++index)
            {
                PadState& pad = pads[index];
                if (pad.handle.load() < 0 || pad.user < 0)
                    continue;
                if (std::find(users.begin(), users.end(), pad.user) ==
                    users.end())
                {
                    ps4_boot_tracef(
                        "port: slot %d: user %d signed out; closing pad",
                        index, pad.user);
                    disconnect_pad(index);
                }
            }
        }

        for (int index = 0; index < kPadCount; ++index)
        {
            PadState& pad = pads[index];
            if (pad.handle.load() < 0 && !open_pad(index, now, users))
                continue;

            OrbisPadData data = {};
            const int32_t handle = pad.handle.load();
            const int32_t read_result =
                handle < 0 ? -1 : scePadReadState(handle, &data);
            if (handle < 0 || read_result != 0 || !data.connected)
            {
                const bool was_connected = pad.read_ok.exchange(false);
                if (read_result != 0)
                {
                    // The handle itself is bad.  Close it and let the next
                    // pass open one fresh handle for this user.
                    trace_read_failure(index, handle, read_result,
                                       data.connected);
                    disconnect_pad(index);
                }
                else if (!data.connected)
                {
                    // The pad is simply off or asleep.  Keep its handle so
                    // one user maps to one slot; closing it here makes the
                    // other empty slots reopen the same user every second.
                    if (was_connected)
                    {
                        ps4_boot_tracef(
                            "port: slot %d: ScePad handle %d disconnected; "
                            "holding it",
                            index, handle);
                        release_pad(index);
                    }
                }
                continue;
            }
            // Announce the pad only after the kernel confirms a controller
            // is really attached to this handle.
            pad.read_ok.store(true);

            const bool cross = (data.buttons & ORBIS_PAD_BUTTON_CROSS) != 0;
            const bool circle = (data.buttons & ORBIS_PAD_BUTTON_CIRCLE) != 0;
            const bool square = (data.buttons & ORBIS_PAD_BUTTON_SQUARE) != 0;
            const bool triangle =
                (data.buttons & ORBIS_PAD_BUTTON_TRIANGLE) != 0;
            const bool l1 = (data.buttons & ORBIS_PAD_BUTTON_L1) != 0;
            const bool l2 = (data.buttons & ORBIS_PAD_BUTTON_L2) != 0;
            const bool r2 = (data.buttons & ORBIS_PAD_BUTTON_R2) != 0;
            const bool options =
                (data.buttons & ORBIS_PAD_BUTTON_OPTIONS) != 0;

            bool held[PS4_BUTTON_COUNT] = {};
            held[PS4_CROSS] = cross;
            held[PS4_CIRCLE] = circle;
            held[PS4_SQUARE] = square;
            held[PS4_TRIANGLE] = triangle;
            held[PS4_L1] = l1;
            held[PS4_L2] = l2;
            held[PS4_R2] = r2;
            held[PS4_OPTIONS] = options;
            held[PS4_MENU_BACK] = !in_game && (circle || options);

            const bool dpad_left =
                (data.buttons & ORBIS_PAD_BUTTON_LEFT) != 0;
            const bool dpad_right =
                (data.buttons & ORBIS_PAD_BUTTON_RIGHT) != 0;
            const bool dpad_up = (data.buttons & ORBIS_PAD_BUTTON_UP) != 0;
            const bool dpad_down =
                (data.buttons & ORBIS_PAD_BUTTON_DOWN) != 0;
            int x = stick_axis(data.leftStick.x, dpad_left, dpad_right,
                               in_game);
            int y = stick_axis(data.leftStick.y, dpad_up, dpad_down,
                               in_game);

            if (!pad.have_previous)
            {
                for (int button = 0; button < PS4_BUTTON_COUNT; ++button)
                    pad.previous_buttons[button] = held[button];
                pad.previous_x = x;
                pad.previous_y = y;
                pad.have_previous = true;
                continue;
            }

            // Cross on a focused text field opens the PS4 keyboard instead of
            // selecting the menu item behind it.  Only player 1 may type.
            if (index == 0 && cross && !pad.previous_buttons[PS4_CROSS] &&
                !in_game && ps4_ime_text_field_active() &&
                ps4_ime_request_open())
            {
                pad.cross_suppressed = true;
                ps4_boot_trace("port: Cross on a focused text field; IME requested");
            }

            if (index == 0 && pad.cross_suppressed)
            {
                if (!cross)
                    pad.cross_suppressed = false;
                pad.previous_buttons[PS4_CROSS] = cross;
            }
            else if (cross != pad.previous_buttons[PS4_CROSS])
            {
                pad.previous_buttons[PS4_CROSS] = cross;
                queue_button(index, PS4_CROSS, cross);
            }

            for (int button = 1; button < PS4_BUTTON_COUNT; ++button)
            {
                if (held[button] == pad.previous_buttons[button])
                    continue;
                pad.previous_buttons[button] = held[button];
                queue_button(index, button, held[button]);
            }

            if (x != pad.previous_x)
            {
                pad.previous_x = x;
                queue_axis(index, 0, x);
            }
            if (y != pad.previous_y)
            {
                pad.previous_y = y;
                queue_axis(index, 1, y);
            }
        }

        SDL_Delay(16);
    }
}   // input_thread
} // namespace

extern "C" void ps4_input_start(void)
{
    if (started.exchange(true))
        return;
    ps4_boot_trace("port: starting the input thread");
    std::thread(input_thread).detach();
}

// Called from InputManager::update() on the main thread.  Registers newly
// connected pads and drains the events produced by the polling thread.
extern "C" void ps4_input_update(InputManager* manager)
{
    if (manager == nullptr)
        return;

    register_pads(manager);

    std::deque<PadEvent> events;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        events.swap(queue);
    }

    for (const PadEvent& event : events)
    {
        if (input_manager != manager)
            break;
        manager->dispatchInput(event.type, event.device, event.button,
                               event.direction, event.value);
    }
}

// Main-thread count used by the controls screen.  It counts registered
// devices, not raw pad handles, so the list rebuilds when the first update
// finishes registering a newly connected controller.
extern "C" int ps4_input_registered_pads(void)
{
    int count = 0;
    for (int index = 0; index < kPadCount; ++index)
    {
        if (pads[index].registered)
            ++count;
    }
    return count;
}
