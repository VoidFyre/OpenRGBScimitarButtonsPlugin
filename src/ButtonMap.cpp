/*---------------------------------------------------------*\
| ButtonMap.cpp                                             |
|                                                           |
|   Side button to key combination mapping                  |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <linux/input-event-codes.h>
#include "ButtonMap.h"
#include "KeyNames.h"

ButtonMap DefaultButtonMap()
{
    /*-----------------------------------------------------*\
    | The mouse's own hardware mapping: 1 2 3 ... 0 - =     |
    \*-----------------------------------------------------*/
    static const unsigned short defaults[SIDE_BUTTON_COUNT] =
    {
        KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,
        KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL
    };

    ButtonMap map;

    for(unsigned int button_idx = 0; button_idx < SIDE_BUTTON_COUNT; button_idx++)
    {
        map[button_idx] = { defaults[button_idx] };
    }

    return(map);
}

std::string GetKeyName(unsigned short code)
{
    for(unsigned int key_idx = 0; key_idx < key_names_count; key_idx++)
    {
        if(key_names[key_idx].code == code)
        {
            return(key_names[key_idx].name);
        }
    }

    return("Key " + std::to_string(code));
}

std::string KeyComboToString(const KeyCombo& combo)
{
    std::string result;

    for(unsigned short code : combo)
    {
        if(!result.empty())
        {
            result += " + ";
        }

        result += GetKeyName(code);
    }

    return(result);
}

nlohmann::json ButtonMapToJson(const ButtonMap& map)
{
    nlohmann::json buttons = nlohmann::json::array();

    for(const KeyCombo& combo : map)
    {
        buttons.push_back(combo);
    }

    return(buttons);
}

ButtonMap ButtonMapFromJson(const nlohmann::json& json)
{
    ButtonMap map = DefaultButtonMap();

    if(!json.is_array())
    {
        return(map);
    }

    for(unsigned int button_idx = 0; button_idx < SIDE_BUTTON_COUNT && button_idx < json.size(); button_idx++)
    {
        const nlohmann::json& entry = json[button_idx];

        if(!entry.is_array())
        {
            continue;
        }

        KeyCombo combo;

        for(const nlohmann::json& code : entry)
        {
            if(code.is_number_unsigned() && code.get<unsigned int>() > 0 && code.get<unsigned int>() <= KEY_NAMES_MAX_CODE)
            {
                combo.push_back(code.get<unsigned short>());
            }
        }

        map[button_idx] = combo;
    }

    return(map);
}
