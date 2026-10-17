/*---------------------------------------------------------*\
| VirtualKeyboard.h                                         |
|                                                           |
|   uinput keyboard that replays side button presses        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <string>

class VirtualKeyboard
{
public:
    VirtualKeyboard();
    ~VirtualKeyboard();

    bool                Open();
    void                Close();
    bool                IsOpen();
    std::string         GetError();

    void                SendKey(unsigned short code, bool pressed);

private:
    int                 fd;
    std::string         error;
};
