/*---------------------------------------------------------*\
| ScimitarButtonsPlugin.h                                   |
|                                                           |
|   OpenRGB plugin that keeps Corsair Scimitar side buttons |
|   working while OpenRGB controls the mouse's lighting     |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <mutex>
#include <QObject>
#include <QPointer>
#include <QWidget>
#include "OpenRGBPluginInterface.h"
#include "ButtonMap.h"
#include "SideButtonService.h"

class ButtonMapWidget;

class ScimitarButtonsPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "../OpenRGBScimitarButtonsPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ScimitarButtonsPlugin();
    ~ScimitarButtonsPlugin();

    /*-----------------------------------------------------*\
    | OpenRGBPluginInterface                                |
    \*-----------------------------------------------------*/
    OpenRGBPluginInfo   GetPluginInfo()                                 override;
    unsigned int        GetPluginAPIVersion()                           override;

    void                Load(OpenRGBPluginAPIInterface* plugin_api_ptr) override;
    QWidget*            GetWidget()                                     override;
    QMenu*              GetTrayMenu()                                   override;
    void                Unload()                                        override;

    void                OnProfileAboutToLoad()                          override;
    void                OnProfileLoad(nlohmann::json profile_data)      override;
    nlohmann::json      OnProfileSave()                                 override;
    unsigned char*      OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;

    void                ProfileManagerUpdated(unsigned int update_reason)   override;
    void                ResourceManagerUpdated(unsigned int update_reason)  override;
    void                SettingsManagerUpdated(unsigned int update_reason)  override;

    /*-----------------------------------------------------*\
    | Used by the plugin tab                                |
    \*-----------------------------------------------------*/
    ButtonMap           GetButtonMap();
    void                SetButtonMap(const ButtonMap& new_map);
    bool                GetRestoreOnExit();
    void                SetRestoreOnExit(bool restore);
    SideButtonStatus    GetStatus();

private:
    void                LoadSettings();
    void                SaveSettings();
    void                UpdateControlPaths();

    OpenRGBPluginAPIInterface*  api;
    SideButtonService           service;
    QPointer<ButtonMapWidget>   widget;

    std::mutex                  settings_lock;
    ButtonMap                   map;
    bool                        restore_on_exit;
};
