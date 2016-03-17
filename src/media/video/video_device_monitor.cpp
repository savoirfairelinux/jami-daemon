/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include <algorithm>
#include <cassert>
#include <sstream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

#include "manager.h"
#include "media_const.h"
#include "client/videomanager.h"
#include "client/ring_signal.h"
#include "config/yamlparser.h"
#include "logger.h"
#include "video_device_monitor.h"

namespace ring { namespace video {

constexpr const char * const VideoDeviceMonitor::CONFIG_LABEL;

using std::map;
using std::string;
using std::stringstream;;
using std::vector;

vector<string>
VideoDeviceMonitor::getDeviceList() const
{
    vector<string> names;

    for (const auto& dev : devices_)
       names.push_back(dev.name);

    return names;
}

DRing::VideoCapabilities
VideoDeviceMonitor::getCapabilities(const string& name) const
{
    const auto iter = findDeviceByName(name);

    if (iter == devices_.end())
        return DRing::VideoCapabilities();

    return iter->getCapabilities();
}

VideoSettings
VideoDeviceMonitor::getSettings(const string& name)
{
    const auto itd = findPreferencesByName(name);

    if (itd == preferences_.end())
        return VideoSettings();

    return *itd;
}

void
VideoDeviceMonitor::applySettings(const string& name, VideoSettings settings)
{
    const auto iter = findDeviceByName(name);

    if (iter == devices_.end())
        return;

    iter->applySettings(settings);
    overwritePreferences(iter->getSettings());
}

string
VideoDeviceMonitor::getDefaultDevice() const
{
    return defaultDevice_;
}

std::string
VideoDeviceMonitor::getMRLForDefaultDevice() const
{
    const auto it = findDeviceByName(defaultDevice_);
    if(it == std::end(devices_))
        return {};
    static const std::string sep = DRing::Media::VideoProtocolPrefix::SEPARATOR;
    return DRing::Media::VideoProtocolPrefix::CAMERA + sep + it->getSettings().name;
}

void
VideoDeviceMonitor::setDefaultDevice(const std::string& name)
{
    const auto it = findDeviceByName(name);
    if (it != devices_.end())
        defaultDevice_ = it->name;
}

DeviceParams
VideoDeviceMonitor::getDeviceParams(const std::string& name) const
{
    const auto itd = findDeviceByName(name);
    if (itd == devices_.cend())
        return DeviceParams();
    return itd->getDeviceParams();
}

static int
getNumber(const string &name, size_t *sharp)
{
    size_t len = name.length();
    // name is too short to be numbered
    if (len < 3)
        return -1;

    for (size_t c = len; c; --c) {
        if (name[c] == '#') {
            unsigned i;
            if (sscanf(name.substr(c).c_str(), "#%u", &i) != 1)
                return -1;
            *sharp = c;
            return i;
        }
    }

    return -1;
}

static void
giveUniqueName(VideoDevice &dev, const vector<VideoDevice> &devices)
{
start:
    for (auto &item : devices) {
        if (dev.name == item.name) {
            size_t sharp;
            int num = getNumber(dev.name, &sharp);
            if (num < 0) // not numbered
                dev.name += " #0";
            else {
                stringstream ss;
                ss  << num + 1;
                dev.name.replace(sharp + 1, ss.str().length(), ss.str());
            }
            goto start; // we changed the name, let's look again if it is unique
        }
    }
}

static void
notify()
{
    if (!Manager::initialized) {
        RING_WARN("Manager not initialized yet");
        return;
    }
    emitSignal<DRing::VideoSignal::DeviceEvent>();
}

void
VideoDeviceMonitor::addDevice(const string& node)
{
    if (findDeviceByNode(node) != devices_.end())
        return;

    // instantiate a new unique device
    try {
        VideoDevice dev {node};
        if (dev.getChannelList().empty())
            return;

        giveUniqueName(dev, devices_);

        // restore its preferences if any, or store the defaults
        auto it = findPreferencesByName(dev.name);
        if (it != preferences_.end())
            dev.applySettings(*it);
        else {
            dev.applySettings(dev.getDefaultSettings());
            preferences_.emplace_back(dev.getSettings());
        }

        // in case there is no default device on a fresh run
        if (defaultDevice_.empty())
            defaultDevice_ = dev.name;

        devices_.emplace_back(std::move(dev));
        notify();
    } catch (const std::exception& e) {
        RING_ERR("Failed to add device %s: %s", node.c_str(), e.what());
        return;
    }
}

void
VideoDeviceMonitor::removeDevice(const string& node)
{
    const auto it = findDeviceByNode(node);

    if (it == devices_.end())
        return;

    if (defaultDevice_ == it->name)
        defaultDevice_.clear();

    devices_.erase(it);
    notify();
}

vector<VideoDevice>::iterator
VideoDeviceMonitor::findDeviceByName(const string& name)
{
    vector<VideoDevice>::iterator it;

    for (it = devices_.begin(); it != devices_.end(); ++it)
        if (it->name == name)
            break;

    return it;
}

vector<VideoDevice>::const_iterator
VideoDeviceMonitor::findDeviceByName(const string& name) const
{
    vector<VideoDevice>::const_iterator it;

    for (it = devices_.cbegin(); it != devices_.cend(); ++it)
        if (it->name == name)
            break;

    return it;
}

vector<VideoDevice>::iterator
VideoDeviceMonitor::findDeviceByNode(const string& node)
{
    vector<VideoDevice>::iterator it;

    for (it = devices_.begin(); it != devices_.end(); ++it)
        if (it->getNode() == node)
            break;

    return it;
}

vector<VideoDevice>::const_iterator
VideoDeviceMonitor::findDeviceByNode(const string& node) const
{
    vector<VideoDevice>::const_iterator it;

    for (it = devices_.cbegin(); it != devices_.cend(); ++it)
        if (it->getNode() == node)
            break;

    return it;
}

vector<VideoSettings>::iterator
VideoDeviceMonitor::findPreferencesByName(const string& name)
{
    for (auto it = preferences_.begin(); it != preferences_.end(); ++it)
        if (it->name == name) return it;
    return preferences_.end();
}

void
VideoDeviceMonitor::overwritePreferences(VideoSettings settings)
{
    auto it = findPreferencesByName(settings.name);
    if (it != preferences_.end())
        preferences_.erase(it);
    preferences_.push_back(settings);
}

void
VideoDeviceMonitor::serialize(YAML::Emitter &out)
{
    out << YAML::Key << CONFIG_LABEL << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "devices" << YAML::Value << preferences_;
    out << YAML::EndMap;
}

void
VideoDeviceMonitor::unserialize(const YAML::Node &in)
{
    const auto &node = in[CONFIG_LABEL];

    /* load the device list from the "video" YAML section */
    const auto& devices = node["devices"];
    for (const auto& dev : devices) {
        VideoSettings pref = dev.as<VideoSettings>();
        if (pref.name.empty())
            continue; // discard malformed section
        overwritePreferences(pref);
        auto itd = findDeviceByName(pref.name);
        if (itd != devices_.end())
            itd->applySettings(pref);
    }

    // Restore the default device if present, or select the first one
    const string pref = preferences_.empty() ? "" : preferences_[0].name;
    const string first = devices_.empty() ? "" : devices_[0].name;
    if (findDeviceByName(pref) != devices_.end())
        defaultDevice_ = pref;
    else
        defaultDevice_ = first;
}

}} // namespace ring::video
