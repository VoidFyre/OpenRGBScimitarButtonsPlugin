/*---------------------------------------------------------*\
| SideButtonService.h                                       |
|                                                           |
|   Reads Corsair Scimitar side buttons while OpenRGB holds |
|   the mouse in software mode and types the mapped keys    |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "ButtonMap.h"
#include "VirtualKeyboard.h"

struct SideButtonStatus
{
    bool                mouse_found;        /* OpenRGB manages a supported mouse    */
    bool                reading;            /* Input interface is open              */
    bool                keyboard_ok;        /* uinput keyboard was created          */
    std::string         error;              /* Last error, empty if none            */
};

class SideButtonService
{
public:
    typedef std::function<void(unsigned int button_idx, bool pressed)> ButtonCallback;
    typedef std::function<void()>                                      StatusCallback;

    SideButtonService();
    ~SideButtonService();

    void                Start();
    void                Stop();

    void                SetButtonMap(const ButtonMap& new_map);
    void                SetButtonCallback(ButtonCallback callback);
    void                SetStatusCallback(StatusCallback callback);

    /*-----------------------------------------------------*\
    | hidraw paths of the mice OpenRGB currently controls.  |
    | Setting them (re)sends the key input table.           |
    \*-----------------------------------------------------*/
    void                SetControlPaths(const std::vector<std::string>& paths);
    void                RestoreHardwareMode();

    SideButtonStatus    GetStatus();

    static bool         IsSupportedControlPath(const std::string& hidraw_path);

private:
    void                Run();
    void                ProcessReport(const unsigned char* report, int length);
    void                ReleaseAll();
    void                UpdateStatus(bool mouse_found, bool reading, const std::string& error);

    std::thread         worker;
    std::atomic<bool>   running;
    std::atomic<bool>   apply_pending;

    std::mutex          lock;
    ButtonMap           map;
    std::vector<std::string> control_paths;
    ButtonCallback      button_callback;
    StatusCallback      status_callback;
    SideButtonStatus    status;

    VirtualKeyboard     keyboard;

    /*-----------------------------------------------------*\
    | Held buttons per report ID (0x01 HID, 0x03 Corsair)   |
    | and the combo each held button pressed                |
    \*-----------------------------------------------------*/
    unsigned int        held_hid;
    unsigned int        held_corsair;
    unsigned int        held;
    ButtonMap           pressed_combos;
};
