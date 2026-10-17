/*---------------------------------------------------------*\
| ButtonMap.h                                               |
|                                                           |
|   Side button to key combination mapping                  |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <array>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#define SIDE_BUTTON_COUNT   12

/*---------------------------------------------------------*\
| Linux key codes, pressed in order and released in reverse |
\*---------------------------------------------------------*/
typedef std::vector<unsigned short>                 KeyCombo;
typedef std::array<KeyCombo, SIDE_BUTTON_COUNT>     ButtonMap;

ButtonMap       DefaultButtonMap();
std::string     KeyComboToString(const KeyCombo& combo);
nlohmann::json  ButtonMapToJson(const ButtonMap& map);
ButtonMap       ButtonMapFromJson(const nlohmann::json& json);
