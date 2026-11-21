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
// The IME starts with the focused text box's current value, so the player can
// backspace and edit it instead of typing the whole thing again.  This applies
// to password fields too: they are ordinary editable text boxes, only drawn
// with a mask.  On accept, the edited value is written back to the same text
// box (replacing the old contents, never appending) and a Return key is
// pushed, so the edit box submits it the same way a hardware keyboard would.
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

#include "guiengine/engine.hpp"
#include "guiengine/widgets/text_box_widget.hpp"
#include <IGUIEditBox.h>

namespace
{
// STK's text boxes are at most a few dozen characters.  256 UTF-16 units
// leaves room to show an existing value in full while still bounding the
// system buffer.  The actual IME limit is the field's own max where set.
constexpr size_t kMaxChars = 256;
constexpr float kScreenWidth = 1920.0f;
constexpr float kScreenHeight = 1080.0f;

const char16_t* const kPlaceholder = u"Player name";
// The IME is shared by every text field, so a fixed title would be wrong.
const char16_t* const kTitle = u"";

std::atomic<bool> text_field_active{false};
std::atomic<bool> open_requested{false};
std::atomic<bool> dialog_active{false};
std::atomic<bool> module_loaded{false};
std::atomic<bool> cancel_requested{false};
int32_t ime_user = -1;
char16_t text_buffer[kMaxChars + 1];
int focused_text_box_id = -1;
size_t active_max_chars = kMaxChars;
// Keep the field that opened the dialog.  Re-querying the GUI focus after the
// system dialog closes can return null (the dialog took focus), which used to
// discard an otherwise valid result.
GUIEngine::TextBoxWidget* active_text_box = nullptr;

GUIEngine::TextBoxWidget* focused_text_box()
{
    return dynamic_cast<GUIEngine::TextBoxWidget*>(
        GUIEngine::getFocusForPlayer(0));
}

irr::gui::IGUIEditBox* focused_edit_box()
{
    GUIEngine::TextBoxWidget* text_box = focused_text_box();
    return text_box ? text_box->getIrrlichtElement<irr::gui::IGUIEditBox>()
                    : nullptr;
}

bool append_utf16(char16_t* out, size_t& length, size_t capacity,
                  uint32_t cp)
{
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        cp = 0xfffd;

    if (cp <= 0xffff)
    {
        if (length >= capacity)
            return false;
        out[length++] = static_cast<char16_t>(cp);
        return true;
    }

    if (length + 2 > capacity)
        return false;
    cp -= 0x10000;
    out[length++] = static_cast<char16_t>(0xd800 + (cp >> 10));
    out[length++] = static_cast<char16_t>(0xdc00 + (cp & 0x3ff));
    return true;
}

// Copy the focused field into the system IME before it opens, including a
// password field, so the player can always go back and edit what is there.
bool prepare_text_buffer()
{
    std::memset(text_buffer, 0, sizeof(text_buffer));
    focused_text_box_id = -1;
    active_text_box = nullptr;
    active_max_chars = kMaxChars;

    GUIEngine::TextBoxWidget* text_box = focused_text_box();
    irr::gui::IGUIEditBox* edit_box = focused_edit_box();
    if (!text_box || !edit_box)
    {
        ps4_boot_trace("port: IME no focused edit box");
        return false;
    }

    active_text_box = text_box;
    focused_text_box_id = text_box->getID();
    const irr::u32 field_max = edit_box->getMax();
    if (field_max > 0 && field_max < active_max_chars)
        active_max_chars = field_max;

    const irr::core::stringw current = edit_box->getText();
    size_t length = 0;
    for (size_t i = 0; i < current.size() && length < active_max_chars; ++i)
    {
        if (!append_utf16(text_buffer, length, active_max_chars,
                          static_cast<uint32_t>(current[i])))
            break;
    }
    text_buffer[length] = 0;
    ps4_boot_tracef("port: IME prefilled %d UTF-16 units%s",
                    static_cast<int>(length),
                    edit_box->isPasswordBox() ? " (password)" : "");
    return true;
}

// Replace the field's contents with the edited IME buffer.  The edit box's
// Return handling will then fire onTextUpdated/onEnterPressed as usual.
bool apply_text_buffer()
{
    GUIEngine::TextBoxWidget* text_box = active_text_box;
    if (!text_box || text_box->getID() != focused_text_box_id)
    {
        ps4_boot_trace("port: IME field disappeared; result discarded");
        return false;
    }
    irr::gui::IGUIEditBox* edit_box =
        text_box->getIrrlichtElement<irr::gui::IGUIEditBox>();
    if (!edit_box)
    {
        ps4_boot_trace("port: IME edit box disappeared; result discarded");
        return false;
    }

    const irr::u32 field_max = edit_box->getMax();
    std::wstring text;
    for (size_t i = 0; i < kMaxChars && text_buffer[i] != 0; ++i)
    {
        uint32_t cp = text_buffer[i];

        // The IME can return a surrogate pair for characters outside the BMP.
        if (cp >= 0xd800 && cp <= 0xdbff)
        {
            if (i + 1 < kMaxChars && text_buffer[i + 1] >= 0xdc00 &&
                text_buffer[i + 1] <= 0xdfff)
            {
                cp = 0x10000 + ((cp - 0xd800) << 10) +
                     (text_buffer[i + 1] - 0xdc00);
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

        if (field_max > 0 && text.size() >= field_max)
            break;
        text.push_back(static_cast<wchar_t>(cp));
    }

    // Replace, never append.  Using the widget also keeps its cached text in
    // sync; the old SDL_TEXTINPUT path appended to whatever was already there,
    // which made a hidden password field grow on every attempt.
    text_box->setText(irr::core::stringw(text.c_str()));
    ps4_boot_tracef("port: IME applied %d characters%s",
                    static_cast<int>(text.size()),
                    edit_box->isPasswordBox() ? " (password)" : "");
    return true;
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
    if (!prepare_text_buffer())
    {
        open_requested.store(false);
        return;
    }

    // The IME library is not loaded by default. Calling sceImeDialogInit()
    // without it ends the app without a user-space signal or crash log.
    if (!module_loaded.load())
    {
        const int32_t load_result = sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
        ps4_boot_tracef("port: IME module load -> 0x%x",
                        static_cast<uint32_t>(load_result));
        if (load_result < 0)
        {
            focused_text_box_id = -1;
            active_text_box = nullptr;
            return;
        }
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

    cancel_requested.store(false);

    OrbisImeDialogSetting setting = {};
    setting.userId = static_cast<uint32_t>(ime_user);
    setting.type = ORBIS_TYPE_DEFAULT;
    setting.supportedLanguages = 0;
    setting.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT;
    setting.inputMethod = ORBIS__DEFAULT;
    setting.option = 0;
    setting.maxTextLength = static_cast<uint32_t>(active_max_chars);
    // The header's wchar_t* fields carry SceWChar16 data on the console.
    setting.inputTextBuffer = reinterpret_cast<wchar_t*>(text_buffer);
    setting.horizontalAlignment = ORBIS_H_CENTER;
    setting.verticalAlignment = ORBIS_V_CENTER;
    // posx/posy default to (0,0), which put the keyboard in the top-left
    // corner, mostly offscreen. With centre alignment these are the
    // anchor point, so use the middle of the 1920x1080 VideoOut surface.
    setting.posx = kScreenWidth * 0.5f;
    setting.posy = kScreenHeight * 0.5f;
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
        focused_text_box_id = -1;
        active_text_box = nullptr;
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
        focused_text_box_id = -1;
        active_text_box = nullptr;
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

    if (!cancelled && get_result >= 0 && result.endstatus == ORBIS_DIALOG_OK &&
        apply_text_buffer())
    {
        push_return();
    }
    focused_text_box_id = -1;
    active_text_box = nullptr;
}
