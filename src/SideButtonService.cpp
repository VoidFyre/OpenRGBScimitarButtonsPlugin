/*---------------------------------------------------------*\
| SideButtonService.cpp                                     |
|                                                           |
|   Reads Corsair Scimitar side buttons while OpenRGB holds |
|   the mouse in software mode and types the mapped keys    |
|                                                           |
|   OpenRGB puts Corsair mice into software mode (07 04 02) |
|   to control their lighting. That stops the firmware from |
|   sending the side buttons' built-in keys. This service   |
|   sends the key input table (07 40) so the side buttons   |
|   are reported as Corsair input reports, reads them from  |
|   the mouse's first HID interface and replays the mapped  |
|   keys through uinput.                                    |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include "SideButtonService.h"

/*---------------------------------------------------------*\
| Corsair Scimitar RGB Elite (wired)                        |
\*---------------------------------------------------------*/
#define CORSAIR_HID_ID              "HID_ID=0003:00001B1C:00001B8B"
#define CORSAIR_PACKET_LENGTH       65
#define CORSAIR_COMMAND_WRITE       0x07
#define CORSAIR_FIELD_SPECIAL       0x04
#define CORSAIR_FIELD_KEYINPUT      0x40
#define CORSAIR_MODE_HARDWARE       0x01
#define CORSAIR_INPUT_HID           0x80
#define CORSAIR_INPUT_CORSAIR       0x40
#define SCIMITAR_BUTTON_IDS         20      /* IDs 1-8 main buttons, 9-20 side   */
#define SCIMITAR_FIRST_SIDE_ID      9

#define REPORT_ID_HID_MOUSE         0x01
#define REPORT_ID_CORSAIR_INPUT     0x03
#define SIDE_BUTTON_MASK            0x0FFF

#define POLL_INTERVAL_MS            250
#define RETRY_INTERVAL_MS           1000

/*---------------------------------------------------------*\
| sysfs helpers                                             |
\*---------------------------------------------------------*/
static std::string ReadFile(const std::string& path)
{
    std::ifstream file(path);
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return(contents);
}

static std::string RealPath(const std::string& path)
{
    char resolved[PATH_MAX];

    if(realpath(path.c_str(), resolved) == NULL)
    {
        return("");
    }

    return(resolved);
}

static std::string ParentDir(const std::string& path)
{
    std::size_t slash = path.find_last_of('/');

    return(slash == std::string::npos ? "" : path.substr(0, slash));
}

static std::string HidrawName(const std::string& hidraw_path)
{
    std::size_t slash = hidraw_path.find_last_of('/');

    return(slash == std::string::npos ? hidraw_path : hidraw_path.substr(slash + 1));
}

/*---------------------------------------------------------*\
| Find the hidraw node of interface 0 (mouse, keyboard and  |
| Corsair input reports) on the same USB device as the      |
| control interface OpenRGB uses                            |
\*---------------------------------------------------------*/
static std::string FindInputPath(const std::string& control_path)
{
    std::string control_dev = RealPath("/sys/class/hidraw/" + HidrawName(control_path) + "/device");
    std::string usb_dev     = ParentDir(ParentDir(control_dev));

    if(usb_dev.empty())
    {
        return("");
    }

    DIR* dir = opendir("/sys/class/hidraw");

    if(dir == NULL)
    {
        return("");
    }

    std::string result;

    while(struct dirent* entry = readdir(dir))
    {
        if(strncmp(entry->d_name, "hidraw", 6) != 0)
        {
            continue;
        }

        std::string dev   = RealPath(std::string("/sys/class/hidraw/") + entry->d_name + "/device");
        std::string iface = ParentDir(dev);

        if(ParentDir(iface) == usb_dev && ReadFile(iface + "/bInterfaceNumber").rfind("00", 0) == 0)
        {
            result = std::string("/dev/") + entry->d_name;
            break;
        }
    }

    closedir(dir);

    return(result);
}

static bool WritePacket(const std::string& hidraw_path, const unsigned char* packet)
{
    int fd = open(hidraw_path.c_str(), O_WRONLY | O_CLOEXEC);

    if(fd < 0)
    {
        return(false);
    }

    bool ok = (write(fd, packet, CORSAIR_PACKET_LENGTH) == CORSAIR_PACKET_LENGTH);

    close(fd);

    return(ok);
}

static bool SendKeyInputTable(const std::string& control_path)
{
    unsigned char packet[CORSAIR_PACKET_LENGTH];

    memset(packet, 0x00, sizeof(packet));

    packet[0x01]    = CORSAIR_COMMAND_WRITE;
    packet[0x02]    = CORSAIR_FIELD_KEYINPUT;
    packet[0x03]    = SCIMITAR_BUTTON_IDS;

    /*-----------------------------------------------------*\
    | <button id> <input mode> pairs from byte 5. Main      |
    | buttons stay HID so clicks, wheel and DPI keep        |
    | working; side buttons become Corsair input reports.   |
    \*-----------------------------------------------------*/
    for(unsigned int button_id = 1; button_id <= SCIMITAR_BUTTON_IDS; button_id++)
    {
        packet[(button_id * 2) + 3] = button_id;
        packet[(button_id * 2) + 4] = (button_id < SCIMITAR_FIRST_SIDE_ID) ? CORSAIR_INPUT_HID : CORSAIR_INPUT_CORSAIR;
    }

    return(WritePacket(control_path, packet));
}

bool SideButtonService::IsSupportedControlPath(const std::string& hidraw_path)
{
    std::string sys_dev = "/sys/class/hidraw/" + HidrawName(hidraw_path) + "/device";

    if(ReadFile(sys_dev + "/uevent").find(CORSAIR_HID_ID) == std::string::npos)
    {
        return(false);
    }

    return(ReadFile(sys_dev + "/../bInterfaceNumber").rfind("01", 0) == 0);
}

/*---------------------------------------------------------*\
| SideButtonService                                         |
\*---------------------------------------------------------*/
SideButtonService::SideButtonService()
{
    running         = false;
    apply_pending   = false;
    map             = DefaultButtonMap();
    status          = { false, false, false, "" };
    held_hid        = 0;
    held_corsair    = 0;
    held            = 0;
}

SideButtonService::~SideButtonService()
{
    Stop();
}

void SideButtonService::Start()
{
    if(running)
    {
        return;
    }

    if(!keyboard.Open())
    {
        UpdateStatus(false, false, keyboard.GetError());
    }

    running = true;
    worker  = std::thread(&SideButtonService::Run, this);
}

void SideButtonService::Stop()
{
    running = false;

    if(worker.joinable())
    {
        worker.join();
    }

    keyboard.Close();
}

void SideButtonService::SetButtonMap(const ButtonMap& new_map)
{
    std::lock_guard<std::mutex> guard(lock);

    map = new_map;
}

void SideButtonService::SetButtonCallback(ButtonCallback callback)
{
    std::lock_guard<std::mutex> guard(lock);

    button_callback = callback;
}

void SideButtonService::SetStatusCallback(StatusCallback callback)
{
    std::lock_guard<std::mutex> guard(lock);

    status_callback = callback;
}

void SideButtonService::SetControlPaths(const std::vector<std::string>& paths)
{
    {
        std::lock_guard<std::mutex> guard(lock);

        control_paths = paths;
    }

    apply_pending = true;
}

void SideButtonService::RestoreHardwareMode()
{
    std::vector<std::string> paths;

    {
        std::lock_guard<std::mutex> guard(lock);

        paths = control_paths;
    }

    unsigned char packet[CORSAIR_PACKET_LENGTH];

    memset(packet, 0x00, sizeof(packet));

    packet[0x01]    = CORSAIR_COMMAND_WRITE;
    packet[0x02]    = CORSAIR_FIELD_SPECIAL;
    packet[0x03]    = CORSAIR_MODE_HARDWARE;

    for(const std::string& path : paths)
    {
        WritePacket(path, packet);
    }
}

SideButtonStatus SideButtonService::GetStatus()
{
    std::lock_guard<std::mutex> guard(lock);

    SideButtonStatus result = status;

    result.keyboard_ok = keyboard.IsOpen();

    return(result);
}

void SideButtonService::UpdateStatus(bool mouse_found, bool reading, const std::string& error)
{
    StatusCallback callback;

    {
        std::lock_guard<std::mutex> guard(lock);

        if(status.mouse_found == mouse_found && status.reading == reading && status.error == error)
        {
            return;
        }

        status.mouse_found  = mouse_found;
        status.reading      = reading;
        status.error        = error;
        callback            = status_callback;
    }

    if(callback)
    {
        callback();
    }
}

void SideButtonService::Run()
{
    int         input_fd    = -1;
    std::string input_path;

    while(running)
    {
        std::vector<std::string> paths;

        {
            std::lock_guard<std::mutex> guard(lock);

            paths = control_paths;
        }

        /*-------------------------------------------------*\
        | Send the key input table after OpenRGB (re)sets   |
        | up the mouse                                      |
        \*-------------------------------------------------*/
        std::string error = keyboard.IsOpen() ? "" : keyboard.GetError();

        if(apply_pending.exchange(false))
        {
            for(const std::string& path : paths)
            {
                if(!SendKeyInputTable(path))
                {
                    error = "Cannot write to " + path + ": " + strerror(errno);
                }
            }
        }

        std::string wanted_path = paths.empty() ? "" : FindInputPath(paths[0]);

        if(input_fd >= 0 && wanted_path != input_path)
        {
            ReleaseAll();
            close(input_fd);
            input_fd = -1;
        }

        if(input_fd < 0 && !wanted_path.empty())
        {
            input_fd = open(wanted_path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);

            if(input_fd >= 0)
            {
                input_path = wanted_path;
            }
            else
            {
                error = "Cannot open " + wanted_path + ": " + strerror(errno);
            }
        }

        UpdateStatus(!paths.empty(), input_fd >= 0, error);

        if(input_fd < 0)
        {
            for(int waited = 0; running && !apply_pending && waited < RETRY_INTERVAL_MS; waited += 50)
            {
                usleep(50 * 1000);
            }
            continue;
        }

        struct pollfd pfd = { input_fd, POLLIN, 0 };

        if(poll(&pfd, 1, POLL_INTERVAL_MS) <= 0)
        {
            continue;
        }

        unsigned char report[64];
        int length = read(input_fd, report, sizeof(report));

        if(length < 0 || (pfd.revents & (POLLERR | POLLHUP)))
        {
            if(length < 0 && errno == EAGAIN)
            {
                continue;
            }

            /*---------------------------------------------*\
            | Mouse unplugged; wait for OpenRGB to set it   |
            | up again                                      |
            \*---------------------------------------------*/
            ReleaseAll();
            close(input_fd);
            input_fd = -1;
            input_path.clear();
            continue;
        }

        ProcessReport(report, length);
    }

    if(input_fd >= 0)
    {
        ReleaseAll();
        close(input_fd);
    }
}

void SideButtonService::ProcessReport(const unsigned char* report, int length)
{
    if(length < 4 || (report[0] != REPORT_ID_HID_MOUSE && report[0] != REPORT_ID_CORSAIR_INPUT))
    {
        return;
    }

    /*-----------------------------------------------------*\
    | Both reports carry side button N at bit N-1 of bytes  |
    | 2-3 (HID buttons 9-20 or Corsair side button bits)    |
    \*-----------------------------------------------------*/
    unsigned int mask = (report[2] | (report[3] << 8)) & SIDE_BUTTON_MASK;

    if(report[0] == REPORT_ID_HID_MOUSE)
    {
        held_hid = mask;
    }
    else
    {
        held_corsair = mask;
    }

    unsigned int now_held = held_hid | held_corsair;
    unsigned int changed  = now_held ^ held;

    if(changed == 0)
    {
        return;
    }

    ButtonMap       current_map;
    ButtonCallback  callback;

    {
        std::lock_guard<std::mutex> guard(lock);

        current_map = map;
        callback    = button_callback;
    }

    for(unsigned int button_idx = 0; button_idx < SIDE_BUTTON_COUNT; button_idx++)
    {
        unsigned int bit = 1 << button_idx;

        if(!(changed & bit))
        {
            continue;
        }

        bool pressed = (now_held & bit) != 0;

        if(pressed)
        {
            pressed_combos[button_idx] = current_map[button_idx];

            for(unsigned short code : pressed_combos[button_idx])
            {
                keyboard.SendKey(code, true);
            }
        }
        else
        {
            const KeyCombo& combo = pressed_combos[button_idx];

            for(auto code = combo.rbegin(); code != combo.rend(); ++code)
            {
                keyboard.SendKey(*code, false);
            }

            pressed_combos[button_idx].clear();
        }

        if(callback)
        {
            callback(button_idx, pressed);
        }
    }

    held = now_held;
}

void SideButtonService::ReleaseAll()
{
    ButtonCallback callback;

    {
        std::lock_guard<std::mutex> guard(lock);

        callback = button_callback;
    }

    for(unsigned int button_idx = 0; button_idx < SIDE_BUTTON_COUNT; button_idx++)
    {
        const KeyCombo& combo = pressed_combos[button_idx];

        for(auto code = combo.rbegin(); code != combo.rend(); ++code)
        {
            keyboard.SendKey(*code, false);
        }

        pressed_combos[button_idx].clear();

        if(callback && (held & (1 << button_idx)))
        {
            callback(button_idx, false);
        }
    }

    held_hid        = 0;
    held_corsair    = 0;
    held            = 0;
}
