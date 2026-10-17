#-----------------------------------------------------------------------------------------------#
# OpenRGB Scimitar Buttons Plugin QMake Project                                                 #
#-----------------------------------------------------------------------------------------------#

QT +=                                                                                           \
    core                                                                                        \
    gui                                                                                         \
    widgets                                                                                     \

TEMPLATE    = lib
TARGET      = OpenRGBScimitarButtonsPlugin

CONFIG +=                                                                                       \
    plugin                                                                                      \
    silent                                                                                      \
    c++17                                                                                       \

#-----------------------------------------------------------------------------------------------#
# Version and build information                                                                 #
#-----------------------------------------------------------------------------------------------#
VERSION_STR     = 1.0.0
PROJECT_URL     = https://github.com/VoidFyre/OpenRGBScimitarButtonsPlugin
GIT_COMMIT_ID   = $$system(git --git-dir $$_PRO_FILE_PWD_/.git --work-tree $$_PRO_FILE_PWD_ rev-parse HEAD 2>/dev/null)

DEFINES +=                                                                                      \
    VERSION_STRING=\\"\"\"$$VERSION_STR\\"\"\"                                                  \
    GIT_COMMIT_ID=\\"\"\"$$GIT_COMMIT_ID\\"\"\"                                                 \
    PROJECT_URL=\\"\"\"$$PROJECT_URL\\"\"\"                                                     \

#-----------------------------------------------------------------------------------------------#
# Sources                                                                                       #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    src/                                                                                        \

HEADERS +=                                                                                      \
    src/ButtonMap.h                                                                             \
    src/ButtonMapWidget.h                                                                       \
    src/KeyNames.h                                                                              \
    src/ScimitarButtonsPlugin.h                                                                 \
    src/SideButtonService.h                                                                     \
    src/VirtualKeyboard.h                                                                       \

SOURCES +=                                                                                      \
    src/ButtonMap.cpp                                                                           \
    src/ButtonMapWidget.cpp                                                                     \
    src/KeyNames.cpp                                                                            \
    src/ScimitarButtonsPlugin.cpp                                                               \
    src/SideButtonService.cpp                                                                   \
    src/VirtualKeyboard.cpp                                                                     \

#-----------------------------------------------------------------------------------------------#
# OpenRGB Plugin SDK (OpenRGB submodule, release_1.0)                                           #
#-----------------------------------------------------------------------------------------------#
INCLUDEPATH +=                                                                                  \
    OpenRGB/                                                                                    \
    OpenRGB/i2c_smbus                                                                           \
    OpenRGB/RGBController                                                                       \
    OpenRGB/net_port                                                                            \
    OpenRGB/dependencies/json                                                                   \

HEADERS +=                                                                                      \
    OpenRGB/OpenRGBPluginInterface.h                                                            \

#-----------------------------------------------------------------------------------------------#
# Linux only: input goes through hidraw and uinput                                              #
#-----------------------------------------------------------------------------------------------#
!linux {
    error("This plugin only supports Linux")
}

isEmpty(PREFIX) {
    PREFIX = /usr
}

target.path = $$PREFIX/lib/openrgb/plugins/

udev.files  = udev/60-openrgb-scimitar-buttons.rules
udev.path   = $$PREFIX/lib/udev/rules.d/

modules.files = udev/openrgb-scimitar-buttons.conf
modules.path  = $$PREFIX/lib/modules-load.d/

INSTALLS += target udev modules

#-----------------------------------------------------------------------------------------------#
# Rebuild the plugin class when its metadata changes                                            #
#-----------------------------------------------------------------------------------------------#
OTHER_FILES += OpenRGBScimitarButtonsPlugin.json
DISTFILES   += OpenRGBScimitarButtonsPlugin.json
