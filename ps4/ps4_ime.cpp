// SuperTuxKart PS4: PS4 system on-screen keyboard for SDL text input.
//
// SDL2 is built with the null video driver here.  That driver has no IME, so
// STK's edit boxes would focus but never receive characters.  Focusing an edit
// box calls SDL_StartTextInput(); that only marks the field as focused.  The
// keyboard must not open by itself (the registration screen focuses its name
// box immediately, which used to pop the keyboard on entry).  The player opens
// it on purpose by pressing Cross while the field is focused: ps4_input.cpp
// calls ps4_ime_request_open(), and ps4_ime_poll() (main thread) opens the
// system dialog.
//
// An accepted result is pushed as SDL_TEXTINPUT, followed by a Return key
// press, so edit boxes that submit on Enter commit the field the same way a
// hardware keyboard would.
//
// The PS4 IME API works in SceWChar16 (UTF-16).  The OpenOrbis headers spell
// those fields wchar_t, but this target's wchar_t is 4 bytes, so the actual
// memory handed to the system must be an explicit 16-bit buffer.  Passing a
// wchar_t buffer would make the system read and write the wrong layout.

#include <SDL.h>
#include <orbis/ImeDialog.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "ps4_kit_diag.h"

#include "guiengine/widget.hpp"

namespace
{
constexpr size_t kMaxChars = 16; // UTF-16 code units; keeps UTF-8 below SDL's 32-byte buffer
constexpr size_t kMaxUtf8 = 31;

const char16_t* const kPlaceholder = u"Player name";
const char16_t* const kTitle = u"Enter your name";

std::atomic<bool> text_field_active{false};
std::atomic<bool> open_requested{false};
std::atomic<bool> dialog_active{false};
std::atomic<bool> module_loaded{false};
std::atomic<bool> cancel_requested{false};
int32_t ime_user = -1;
char16_t text_buffer[kMaxChars + 1];

std::string to_utf8(const char16_t* text)
{
    std::string out;
    if (!text)
        return out;

    for (size_t i = 0; i < kMaxChars && text[i] != 0; ++i)
    {
        uint32_t cp = text[i];

        // The IME can return a surrogate pair for characters outside the BMP.
        if (cp >= 0xd800 && cp <= 0xdbff)
        {
            if (i + 1 < kMaxChars && text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff)
            {
                cp = 0x10000 + ((cp - 0xd800) << 10) + (text[i + 1] - 0xdc00);
                ++i;
            }
            else
            {
                cp = 0xfffd;
            }
        }
        else if (cp >= 0xdc00 && cp <= 0xdfff)
        {
            cp = 0xfffd;
        }

        char encoded[4];
        size_t length = 0;
        if (cp < 0x80)
        {
            encoded[0] = static_cast<char>(cp);
            length = 1;
        }
        else if (cp < 0x800)
        {
            encoded[0] = static_cast<char>(0xc0 | (cp >> 6));
            encoded[1] = static_cast<char>(0x80 | (cp & 0x3f));
            length = 2;
        }
        else if (cp < 0x10000)
        {
            encoded[0] = static_cast<char>(0xe0 | (cp >> 12));
            encoded[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            encoded[2] = static_cast<char>(0x80 | (cp & 0x3f));
            length = 3;
        }
        else
        {
            encoded[0] = static_cast<char>(0xf0 | (cp >> 18));
            encoded[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
            encoded[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            encoded[3] = static_cast<char>(0x80 | (cp & 0x3f));
            length = 4;
        }

        if (out.size() + length > kMaxUtf8)
            break;
        out.append(encoded, length);
    }
    return out;
}

void push_text(const std::string& text)
{
    if (text.empty())
        return;

    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_TEXTINPUT;
    event.text.timestamp = SDL_GetTicks();
    event.text.windowID = 0;
    std::memcpy(event.text.text, text.data(), text.size());
    event.text.text[text.size()] = '\0';

    if (SDL_PushEvent(&event) != 1)
        ps4_boot_tracef("port: IME SDL_PushEvent -> \"%s\"", SDL_GetError());
    else
        ps4_boot_tracef("port: IME pushed \"%s\"", text.c_str());
}

// Commit the field: the IME's own Enter button has no SDL key behind it.
void push_return()
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.timestamp = SDL_GetTicks();
    event.key.windowID = 0;
    event.key.state = SDL_PRESSED;
    event.key.repeat = 0;
    event.key.keysym.sym = SDLK_RETURN;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(SDLK_RETURN);
    event.key.keysym.mod = KMOD_NONE;
    SDL_PushEvent(&event);

    SDL_zero(event);
    event.type = SDL_KEYUP;
    event.key.timestamp = SDL_GetTicks();
    event.key.windowID = 0;
    event.key.state = SDL_RELEASED;
    event.key.repeat = 0;
    event.key.keysym.sym = SDLK_RETURN;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(SDLK_RETURN);
    event.key.keysym.mod = KMOD_NONE;
    SDL_PushEvent(&event);

    ps4_boot_trace("port: IME pushed Return");
}

void open_dialog()
{
    // The IME library is not loaded by default. Calling sceImeDialogInit()
    // without it ends the app without a user-space signal or crash log.
    if (!module_loaded.load())
    {
        const int32_t load_result = sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
        ps4_boot_tracef("port: IME module load -> 0x%x",
                        static_cast<uint32_t>(load_result));
        if (load_result < 0)
            return;
        module_loaded.store(true);
    }

    if (ime_user < 0)
    {
        sceUserServiceInitialize(nullptr);
        if (sceUserServiceGetInitialUser(&ime_user) < 0)
        {
            ime_user = 0;
            ps4_boot_trace("port: IME user lookup failed, using user 0");
        }
    }

    std::memset(text_buffer, 0, sizeof(text_buffer));
    cancel_requested.store(false);

    OrbisImeDialogSetting setting = {};
    setting.userId = static_cast<uint32_t>(ime_user);
    setting.type = ORBIS_TYPE_DEFAULT;
    setting.supportedLanguages = 0;
    setting.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT;
    setting.inputMethod = ORBIS__DEFAULT;
    setting.option = 0;
    setting.maxTextLength = static_cast<uint32_t>(kMaxChars);
    // The header's wchar_t* fields carry SceWChar16 data on the console.
    setting.inputTextBuffer = reinterpret_cast<wchar_t*>(text_buffer);
    setting.horizontalAlignment = ORBIS_H_CENTER;
    setting.verticalAlignment = ORBIS_V_CENTER;
    setting.placeholder = reinterpret_cast<const wchar_t*>(kPlaceholder);
    setting.title = reinterpret_cast<const wchar_t*>(kTitle);

    // Pause the pad-to-keyboard bridge for the whole init/run, not just after
    // a successful init: the system dialog owns the controller while it is up.
    dialog_active.store(true);
    ps4_boot_tracef("port: IME opening (user %d)", ime_user);
    const int32_t result = sceImeDialogInit(&setting, nullptr);
    ps4_boot_tracef("port: IME init -> 0x%x", static_cast<uint32_t>(result));
    if (result < 0)
    {
        dialog_active.store(false);
        open_requested.store(false);
        return;
    }
}
} // namespace

extern "C" bool ps4_ime_active(void)
{
    return dialog_active.load();
}

// SDL_StartTextInput's flag alone is not enough: STK starts text input when a
// screen with an edit box is pushed, and the flag can stay set after focus has
// moved to a button. isWithinATextBox() is the GUI's own focused-widget state,
// so a Yes/No button can never be mistaken for a text field.
extern "C" bool ps4_ime_text_field_active(void)
{
    return text_field_active.load() && !dialog_active.load() &&
           GUIEngine::isWithinATextBox();
}

extern "C" bool ps4_ime_request_open(void)
{
    if (!text_field_active.load() || dialog_active.load() ||
        !GUIEngine::isWithinATextBox())
    {
        return false;
    }

    open_requested.store(true);
    return true;
}

extern "C" void __real_SDL_StartTextInput(void);
extern "C" void __real_SDL_StopTextInput(void);

extern "C" void __wrap_SDL_StartTextInput(void)
{
    __real_SDL_StartTextInput();

    // Focus only: the keyboard waits for Cross (ps4_input.cpp).
    if (!text_field_active.exchange(true))
        ps4_boot_trace("port: IME text field focused (press Cross to open)");
}

extern "C" void __wrap_SDL_StopTextInput(void)
{
    const bool was_active = text_field_active.exchange(false);
    open_requested.store(false);

    if (dialog_active.load())
    {
        cancel_requested.store(true);
        sceImeDialogAbort();
        ps4_boot_trace("port: IME abort requested");
    }
    __real_SDL_StopTextInput();

    if (was_active)
        ps4_boot_trace("port: IME text field blurred");
}

extern "C" void ps4_ime_poll(void)
{
    if (!dialog_active.load())
    {
        if (open_requested.exchange(false) && text_field_active.load() &&
            GUIEngine::isWithinATextBox())
        {
            open_dialog();
        }
        return;
    }

    const OrbisDialogStatus status = sceImeDialogGetStatus();
    if (status == ORBIS_DIALOG_STATUS_RUNNING)
        return;

    if (status == ORBIS_DIALOG_STATUS_NONE)
    {
        ps4_boot_trace("port: IME vanished (status NONE)");
        dialog_active.store(false);
        cancel_requested.store(false);
        return;
    }

    OrbisDialogResult result = {};
    const int32_t get_result = sceImeDialogGetResult(&result);
    const int32_t term_result = sceImeDialogTerm();
    dialog_active.store(false);

    const bool cancelled = cancel_requested.exchange(false);
    ps4_boot_tracef("port: IME done -> get 0x%x, term 0x%x, end %d, cancel %d",
                    static_cast<uint32_t>(get_result), static_cast<uint32_t>(term_result),
                    static_cast<int>(result.endstatus), cancelled ? 1 : 0);

    if (!cancelled && get_result >= 0 && result.endstatus == ORBIS_DIALOG_OK)
    {
        push_text(to_utf8(text_buffer));
        push_return();
    }
}
