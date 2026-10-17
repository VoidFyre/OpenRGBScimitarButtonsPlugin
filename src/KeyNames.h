/*---------------------------------------------------------*\
| KeyNames.h                                                |
|                                                           |
|   Display names for Linux input key codes                 |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <string>

struct KeyName
{
    unsigned short  code;
    const char*     name;
};

extern const KeyName        key_names[];
extern const unsigned int   key_names_count;

/*---------------------------------------------------------*\
| Highest key code advertised by the virtual keyboard       |
\*---------------------------------------------------------*/
#define KEY_NAMES_MAX_CODE  248

std::string GetKeyName(unsigned short code);
