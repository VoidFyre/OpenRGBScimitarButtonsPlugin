/*---------------------------------------------------------*\
| VirtualKeyboard.cpp                                       |
|                                                           |
|   uinput keyboard that replays side button presses        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <linux/uinput.h>
#include "VirtualKeyboard.h"
#include "KeyNames.h"

VirtualKeyboard::VirtualKeyboard()
{
    fd = -1;
}

VirtualKeyboard::~VirtualKeyboard()
{
    Close();
}

bool VirtualKeyboard::Open()
{
    if(fd >= 0)
    {
        return(true);
    }

    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);

    if(fd < 0)
    {
        error = std::string("Cannot open /dev/uinput: ") + strerror(errno);
        return(false);
    }

    /*-----------------------------------------------------*\
    | Advertise every typeable key so remapping never needs |
    | the device to be recreated                            |
    \*-----------------------------------------------------*/
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    for(int code = 1; code <= KEY_NAMES_MAX_CODE; code++)
    {
        ioctl(fd, UI_SET_KEYBIT, code);
    }

    struct uinput_setup setup;

    memset(&setup, 0, sizeof(setup));

    setup.id.bustype    = BUS_VIRTUAL;
    setup.id.vendor     = 0x1B1C;
    setup.id.product    = 0x1B8B;
    setup.id.version    = 1;
    strncpy(setup.name, "OpenRGB Scimitar side buttons", UINPUT_MAX_NAME_SIZE - 1);

    if(ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0)
    {
        error = std::string("Cannot create uinput keyboard: ") + strerror(errno);
        close(fd);
        fd = -1;
        return(false);
    }

    error.clear();
    return(true);
}

void VirtualKeyboard::Close()
{
    if(fd >= 0)
    {
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
        fd = -1;
    }
}

bool VirtualKeyboard::IsOpen()
{
    return(fd >= 0);
}

std::string VirtualKeyboard::GetError()
{
    return(error);
}

void VirtualKeyboard::SendKey(unsigned short code, bool pressed)
{
    if(fd < 0)
    {
        return;
    }

    struct input_event events[2];

    memset(events, 0, sizeof(events));
    gettimeofday(&events[0].time, NULL);
    events[1].time      = events[0].time;

    events[0].type      = EV_KEY;
    events[0].code      = code;
    events[0].value     = pressed ? 1 : 0;

    events[1].type      = EV_SYN;
    events[1].code      = SYN_REPORT;
    events[1].value     = 0;

    if(write(fd, events, sizeof(events)) < 0)
    {
        error = std::string("uinput write failed: ") + strerror(errno);
    }
}
