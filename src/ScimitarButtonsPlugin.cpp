/*---------------------------------------------------------*\
| ScimitarButtonsPlugin.cpp                                 |
|                                                           |
|   OpenRGB plugin that keeps Corsair Scimitar side buttons |
|   working while OpenRGB controls the mouse's lighting     |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstring>
#include "ScimitarButtonsPlugin.h"
#include "ButtonMapWidget.h"
#include "LogManager.h"
#include "ResourceManagerCallback.h"

#define SETTINGS_KEY    "ScimitarButtonsPlugin"
#define LOCATION_PREFIX "HID: "

#define PLUGIN_LOG(level, ...)                                              \
    if(api != nullptr)                                                      \
    {                                                                       \
        api->LogEntry(__FILE__, __LINE__, level, "[ScimitarButtons] " __VA_ARGS__); \
    }

ScimitarButtonsPlugin::ScimitarButtonsPlugin()
{
    api             = nullptr;
    map             = DefaultButtonMap();
    restore_on_exit = true;
}

ScimitarButtonsPlugin::~ScimitarButtonsPlugin()
{
    service.Stop();
}

OpenRGBPluginInfo ScimitarButtonsPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name            = "Scimitar Buttons";
    info.Description     = "Keeps the 12 side buttons of the Corsair Scimitar RGB Elite working while OpenRGB controls its lighting, and lets you map them to any key";
    info.Version         = VERSION_STRING;
    info.Commit          = GIT_COMMIT_ID;
    info.URL             = PROJECT_URL;
    info.Location        = OPENRGB_PLUGIN_LOCATION_TOP;
    info.Label           = "Scimitar Buttons";
    info.ProtocolVersion = 0;

    return(info);
}

unsigned int ScimitarButtonsPlugin::GetPluginAPIVersion()
{
    return(OPENRGB_PLUGIN_API_VERSION);
}

void ScimitarButtonsPlugin::Load(OpenRGBPluginAPIInterface* plugin_api_ptr)
{
    api = plugin_api_ptr;

    LoadSettings();

    service.SetButtonMap(GetButtonMap());

    /*-----------------------------------------------------*\
    | The service calls back from its worker thread; hop to |
    | the GUI thread (where this object lives) before       |
    | touching the tab                                      |
    \*-----------------------------------------------------*/
    service.SetStatusCallback([this]()
    {
        QMetaObject::invokeMethod(this, [this]()
        {
            if(widget)
            {
                widget->UpdateStatus();
            }
        }, Qt::QueuedConnection);
    });
    service.SetButtonCallback([this](unsigned int button_idx, bool pressed)
    {
        QMetaObject::invokeMethod(this, [this, button_idx, pressed]()
        {
            if(widget)
            {
                widget->ShowButtonState(button_idx, pressed);
            }
        }, Qt::QueuedConnection);
    });
    service.Start();

    /*-----------------------------------------------------*\
    | Plugins load after the first detection, so pick up    |
    | the mice OpenRGB has already set up                   |
    \*-----------------------------------------------------*/
    UpdateControlPaths();

    if(!service.GetStatus().keyboard_ok)
    {
        PLUGIN_LOG(LL_ERROR, "%s", service.GetStatus().error.c_str());
    }
}

QWidget* ScimitarButtonsPlugin::GetWidget()
{
    widget = new ButtonMapWidget(this);

    return(widget);
}

QMenu* ScimitarButtonsPlugin::GetTrayMenu()
{
    return(nullptr);
}

void ScimitarButtonsPlugin::Unload()
{
    service.SetButtonCallback(nullptr);
    service.SetStatusCallback(nullptr);

    /*-----------------------------------------------------*\
    | Without this plugin the side buttons stay silent      |
    | until the mouse is replugged, so hand them back to    |
    | the mouse's own hardware keys                         |
    \*-----------------------------------------------------*/
    if(GetRestoreOnExit())
    {
        service.RestoreHardwareMode();
    }

    service.Stop();
}

void ScimitarButtonsPlugin::OnProfileAboutToLoad()
{
}

void ScimitarButtonsPlugin::OnProfileLoad(nlohmann::json /*profile_data*/)
{
}

nlohmann::json ScimitarButtonsPlugin::OnProfileSave()
{
    return(nlohmann::json());
}

unsigned char* ScimitarButtonsPlugin::OnSDKCommand(unsigned int /*pkt_id*/, unsigned char* /*pkt_data*/, unsigned int* pkt_size)
{
    *pkt_size = 0;

    return(nullptr);
}

void ScimitarButtonsPlugin::ProfileManagerUpdated(unsigned int /*update_reason*/)
{
}

void ScimitarButtonsPlugin::ResourceManagerUpdated(unsigned int update_reason)
{
    /*-----------------------------------------------------*\
    | Detection (including rescans and hotplug) recreates   |
    | the controller, which puts the mouse back into plain  |
    | software mode, so the table has to be sent again      |
    \*-----------------------------------------------------*/
    if(update_reason == RESOURCEMANAGER_UPDATE_REASON_DETECTION_COMPLETE
    || update_reason == RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED)
    {
        UpdateControlPaths();
    }
}

void ScimitarButtonsPlugin::SettingsManagerUpdated(unsigned int /*update_reason*/)
{
}

void ScimitarButtonsPlugin::UpdateControlPaths()
{
    if(api == nullptr)
    {
        return;
    }

    std::vector<std::string> paths;

    for(RGBControllerInterface* controller : api->GetRGBControllers())
    {
        std::string location = controller->GetLocation();

        if(location.rfind(LOCATION_PREFIX, 0) != 0)
        {
            continue;
        }

        std::string path = location.substr(strlen(LOCATION_PREFIX));

        if(SideButtonService::IsSupportedControlPath(path))
        {
            paths.push_back(path);
        }
    }

    service.SetControlPaths(paths);
}

/*---------------------------------------------------------*\
| Settings                                                  |
\*---------------------------------------------------------*/
void ScimitarButtonsPlugin::LoadSettings()
{
    nlohmann::json settings = api->GetSettings(SETTINGS_KEY);

    std::lock_guard<std::mutex> guard(settings_lock);

    if(settings.contains("buttons"))
    {
        map = ButtonMapFromJson(settings["buttons"]);
    }

    if(settings.contains("restore_hardware_on_exit") && settings["restore_hardware_on_exit"].is_boolean())
    {
        restore_on_exit = settings["restore_hardware_on_exit"];
    }
}

void ScimitarButtonsPlugin::SaveSettings()
{
    nlohmann::json settings;

    {
        std::lock_guard<std::mutex> guard(settings_lock);

        settings["buttons"]                  = ButtonMapToJson(map);
        settings["restore_hardware_on_exit"] = restore_on_exit;
    }

    api->SetSettings(SETTINGS_KEY, settings);
    api->SaveSettings();
}

ButtonMap ScimitarButtonsPlugin::GetButtonMap()
{
    std::lock_guard<std::mutex> guard(settings_lock);

    return(map);
}

void ScimitarButtonsPlugin::SetButtonMap(const ButtonMap& new_map)
{
    {
        std::lock_guard<std::mutex> guard(settings_lock);

        map = new_map;
    }

    service.SetButtonMap(new_map);
    SaveSettings();
}

bool ScimitarButtonsPlugin::GetRestoreOnExit()
{
    std::lock_guard<std::mutex> guard(settings_lock);

    return(restore_on_exit);
}

void ScimitarButtonsPlugin::SetRestoreOnExit(bool restore)
{
    {
        std::lock_guard<std::mutex> guard(settings_lock);

        restore_on_exit = restore;
    }

    SaveSettings();
}

SideButtonStatus ScimitarButtonsPlugin::GetStatus()
{
    return(service.GetStatus());
}
