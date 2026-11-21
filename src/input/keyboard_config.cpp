//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2010-2015 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


#include "input/keyboard_config.hpp"

#include "io/xml_node.hpp"
#include "utils/log.hpp"

#include <SKeyMap.h>

#include <assert.h>

using namespace irr;


KeyboardConfig::KeyboardConfig()
              : DeviceConfig()
{
    setDefaultBinds();
    setPlugged();
}   // KeyboardConfig

// ----------------------------------------------------------------------------
/** Saves the configuration to a file. It writes the name for a gamepad
 *  config, saves the device specific parameters, and calls
 *  DeviceConfig::save() to save the rest.
 *  \param stream The stream to save to.
 */
void KeyboardConfig::save(std::ofstream& stream)
{
    stream << "<keyboard ";
    DeviceConfig::save(stream);
    stream << "</keyboard>\n\n";
}  // save

//------------------------------------------------------------------------------
/** Returns the control to display for an action.  The PS4 port has no
 *  keyboard: the pad thread in ps4/ps4_input.cpp feeds DualShock 4 presses into
 *  STK as SDL key events, so the keyboard configuration is the "latest used
 *  device" that the tutorial, the help screens and the input options read.
 *  Reporting the pad control that produces each action keeps every prompt a
 *  controller prompt instead of the key name (Backspace, Return, Space...) the
 *  player can never press. */
irr::core::stringw KeyboardConfig::getBindingAsString(
                                                 const PlayerAction action) const
{
#ifdef __PS4__
    if (action >= 0 && action < PA_COUNT &&
        m_bindings[action].getType() != Input::IT_NONE)
    {
        // Private-use codepoints backed by data/ttf/DS4Prompts.ttf (listed in
        // the normal-ttf fallback chain of stk_config.xml). Drawing the real
        // button keeps every prompt a controller prompt and tints it with the
        // surrounding text colour.
        switch (action)
        {
        case PA_ACCEL:       return L"\xE000";  // Cross
        case PA_BRAKE:       return L"\xE001";  // Circle
        // Steering is one physical control; both directions return the single
        // left-stick glyph, so the tutorial names it once instead of printing
        // the left and right stick icons next to each other.
        case PA_STEER_LEFT:
        case PA_STEER_RIGHT: return L"\xE009";  // left stick
        case PA_FIRE:        return L"\xE007";  // R2
        case PA_LOOK_BACK:   return L"\xE002";  // Square
        case PA_DRIFT:       return L"\xE005";  // L2
        case PA_NITRO:       return L"\xE004";  // L1
        case PA_RESCUE:      return L"\xE003";  // Triangle
        case PA_PAUSE_RACE:  return L"\xE008";  // Options
        case PA_MENU_UP:     return L"d-pad up";
        case PA_MENU_DOWN:   return L"d-pad down";
        case PA_MENU_LEFT:   return L"d-pad left";
        case PA_MENU_RIGHT:  return L"d-pad right";
        case PA_MENU_SELECT: return L"\xE000";  // Cross
        case PA_MENU_CANCEL: return L"\xE001";  // Circle
        default:             break;
        }
    }
#endif
    return DeviceConfig::getBindingAsString(action);
}   // getBindingAsString

//------------------------------------------------------------------------------

void KeyboardConfig::setDefaultBinds()
{
    setBinding(PA_NITRO,       Input::IT_KEYBOARD, IRR_KEY_N);
    setBinding(PA_ACCEL,       Input::IT_KEYBOARD, IRR_KEY_UP);
    setBinding(PA_BRAKE,       Input::IT_KEYBOARD, IRR_KEY_DOWN);
    setBinding(PA_STEER_LEFT,  Input::IT_KEYBOARD, IRR_KEY_LEFT);
    setBinding(PA_STEER_RIGHT, Input::IT_KEYBOARD, IRR_KEY_RIGHT);
    setBinding(PA_DRIFT,       Input::IT_KEYBOARD, IRR_KEY_V);
    setBinding(PA_RESCUE,      Input::IT_KEYBOARD, IRR_KEY_BACK);
    setBinding(PA_FIRE,        Input::IT_KEYBOARD, IRR_KEY_SPACE);
    setBinding(PA_LOOK_BACK,   Input::IT_KEYBOARD, IRR_KEY_B);
    setBinding(PA_PAUSE_RACE,  Input::IT_KEYBOARD, IRR_KEY_ESCAPE);

    setBinding(PA_MENU_UP,     Input::IT_KEYBOARD, IRR_KEY_UP);
    setBinding(PA_MENU_DOWN,   Input::IT_KEYBOARD, IRR_KEY_DOWN);
    setBinding(PA_MENU_LEFT,   Input::IT_KEYBOARD, IRR_KEY_LEFT);
    setBinding(PA_MENU_RIGHT,  Input::IT_KEYBOARD, IRR_KEY_RIGHT);
    setBinding(PA_MENU_SELECT, Input::IT_KEYBOARD, IRR_KEY_RETURN);
    setBinding(PA_MENU_CANCEL, Input::IT_KEYBOARD, IRR_KEY_ESCAPE);
}

//------------------------------------------------------------------------------
