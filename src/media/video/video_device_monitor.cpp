/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

namespace jami { namespace video {

constexpr const char * const VideoDeviceMonitor::CONFIG_LABEL;

using std::map;
using std::string;
using std::stringstream;
using std::vector;

vector<string>
VideoDeviceMonitor::getDeviceList() const
{
    std::lock_guard<std::mutex> l(lock_);
    vector<string> names;
    names.reserve(devices_.size());
    for (const auto& dev : devices_)
       names.emplace_back(dev.name);
    return names;
}

DRing::VideoCapabilities
VideoDeviceMonitor::getCapabilities(const string& name) const
{
    std::lock_guard<std::mutex> l(lock_);
    const auto iter = findDeviceByName(name);
    if (iter == devices_.end())
        return DRing::VideoCapabilities();

    return iter->getCapabilities();
}
#pragma optimize("", off)
VideoSettings
VideoDeviceMonitor::getSettings(const string& name)
{
    std::lock_guard<std::mutex> l(lock_);

    const auto devIter = findDeviceByName(name);
    if (devIter == devices_.end())
        return VideoSettings();

    const auto node = devIter->getNode();
    const auto prefIter = findPreferencesByNode(node);

    if (prefIter == preferences_.end())
        return VideoSettings();

    return *prefIter;
}
#pragma optimize("", on)
void
VideoDeviceMonitor::applySettings(const string& name, const VideoSettings& settings)
{
    std::lock_guard<std::mutex> l(lock_);
    const auto iter = findDeviceByName(name);

    if (iter == devices_.end())
        return;

    iter->applySettings(settings);
    auto it = findPreferencesByNode(settings.node);
    if (it != preferences_.end())
        (*it) = settings;
}

string
VideoDeviceMonitor::getDefaultDevice() const
{
    std::lock_guard<std::mutex> l(lock_);
    return defaultDevice_;
}
#pragma optimize("", off)
std::string
VideoDeviceMonitor::getMRLForDefaultDevice() const
{
    std::lock_guard<std::mutex> l(lock_);
    const auto it = findDeviceByName(defaultDevice_);
    if(it == std::end(devices_))
        return {};
    static const std::string sep = DRing::Media::VideoProtocolPrefix::SEPARATOR;
    return DRing::Media::VideoProtocolPrefix::CAMERA + sep + it->name;
}
#pragma optimize("", on)
void
VideoDeviceMonitor::setDefaultDevice(const std::string& name)
{
    std::lock_guard<std::mutex> l(lock_);
    const auto itDev = findDeviceByName(name);
    if (itDev != devices_.end()) {
        defaultDevice_ = itDev->name;

        // place it at the begining of the prefs
        auto itPref = findPreferencesByNode(itDev->getNode());
        if (itPref != preferences_.end()) {
            auto settings = *itPref;
            preferences_.erase(itPref);
            preferences_.insert(preferences_.begin(), settings);
        } else {
            preferences_.insert(preferences_.begin(), itDev->getSettings());
        }
    }
}

void
VideoDeviceMonitor::setDeviceOrientation(const std::string& name, int angle)
{
    const auto itd = findDeviceByName(name);
    if (itd != devices_.cend()) {
        itd->setOrientation(angle);
    }
}

DeviceParams
VideoDeviceMonitor::getDeviceParams(const std::string& name) const
{
    std::lock_guard<std::mutex> l(lock_);
    const auto itd = findDeviceByName(name);
    if (itd == devices_.cend())
        return DeviceParams();
    return itd->getDeviceParams();
}

//static int
//getNumber(const string &name, size_t *sharp)
//{
//    size_t len = name.length();
//    // name is too short to be numbered
//    if (len < 3)
//        return -1;
//
//    for (size_t c = len; c; --c) {
//        if (name[c] == '#') {
//            unsigned i;
//            if (sscanf(name.substr(c).c_str(), "#%u", &i) != 1)
//                return -1;
//            *sharp = c;
//            return i;
//        }
//    }
//
//    return -1;
//}

//static void
//giveUniqueName(VideoDevice &dev, const vector<VideoDevice> &devices)
//{
//start:
//    for (auto &item : devices) {
//        if (dev.name == item.name) {
//            size_t sharp;
//            int num = getNumber(dev.name, &sharp);
//            if (num < 0) // not numbered
//                dev.name += " #0";
//            else {
//                stringstream ss;
//                ss  << num + 1;
//                dev.name.replace(sharp + 1, ss.str().length(), ss.str());
//            }
//            goto start; // we changed the name, let's look again if it is unique
//        }
//    }
//}

static void
giveUniqueName(VideoDevice &dev, const vector<VideoDevice> &devices)
{
    std::string suffix;
    uint64_t number = 2;
    auto unique = true;
    for (;; unique = static_cast<bool>(++number)) {
        for (const auto& s : devices)
            unique &= static_cast<bool>(s.name.compare(dev.name + suffix));
        if (unique)
            return (void)(dev.name += suffix);
        suffix = " (" + std::to_string(number) + ")";
    }
}

static void
notify()
{
    if (Manager::initialized) {
        emitSignal<DRing::VideoSignal::DeviceEvent>();
    }
}

void
VideoDeviceMonitor::addDevice(const string& node, const std::vector<std::map<std::string, std::string>>* devInfo)
{
    try {
        std::lock_guard<std::mutex> l(lock_);
        if (findDeviceByNode(node) != devices_.end())
            return;

        // instantiate a new unique device
        VideoDevice dev {node, *devInfo};

        if (dev.getChannelList().empty())
            return;

        giveUniqueName(dev, devices_);

        // restore its preferences if any, or store the defaults
        auto it = findPreferencesByNode(node);
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
    } catch (const std::exception& e) {
        JAMI_ERR("Failed to add device %s: %s", node.c_str(), e.what());
        return;
    }
    notify();
}

void
VideoDeviceMonitor::removeDevice(const string& node)
{
    {
        std::lock_guard<std::mutex> l(lock_);
        const auto it = findDeviceByNode(node);
        if (it == devices_.end())
            return;

        auto removedDeviceName = it->name;
        devices_.erase(it);

        if (defaultDevice_ == removedDeviceName) {
            if (devices_.size() == 0) {
                defaultDevice_.clear();
            } else {
                defaultDevice_ = devices_[0].name;
            }
        }
    }
    notify();
}

vector<VideoDevice>::iterator
VideoDeviceMonitor::findDeviceByName(const string& name)
{
    for (auto it = devices_.begin(); it != devices_.end(); ++it)
        if (it->name == name)
            return it;
    return devices_.end();
}

vector<VideoDevice>::const_iterator
VideoDeviceMonitor::findDeviceByName(const string& name) const
{
    for (auto it = devices_.cbegin(); it != devices_.cend(); ++it)
        if (it->name == name)
            return it;
    return devices_.cend();
}

vector<VideoDevice>::iterator
VideoDeviceMonitor::findDeviceByNode(const string& node)
{
    for (auto it = devices_.begin(); it != devices_.end(); ++it)
        if (it->getNode().find(node) != std::string::npos)
            return it;
    return devices_.end();
}

vector<VideoDevice>::const_iterator
VideoDeviceMonitor::findDeviceByNode(const string& node) const
{
    for (auto it = devices_.cbegin(); it != devices_.cend(); ++it)
        if (it->getNode().find(node) != std::string::npos)
            return it;
    return devices_.end();
}
#pragma optimize("", off)
vector<VideoSettings>::iterator
VideoDeviceMonitor::findPreferencesByNode(const string& node)
{
    for (auto it = preferences_.begin(); it != preferences_.end(); ++it)
        if (it->node.find(node) != std::string::npos)
            return it;
    return preferences_.end();
}
#pragma optimize("", on)
void
VideoDeviceMonitor::overwritePreferences(const VideoSettings& settings)
{
    auto it = findPreferencesByNode(settings.node);
    if (it != preferences_.end())
        preferences_.erase(it);
    preferences_.emplace_back(settings);
}

void
VideoDeviceMonitor::serialize(YAML::Emitter &out) const
{
    std::lock_guard<std::mutex> l(lock_);
    out << YAML::Key << "devices" << YAML::Value << preferences_;
}
#pragma optimize("", off)
void
VideoDeviceMonitor::unserialize(const YAML::Node &in)
{
    std::lock_guard<std::mutex> l(lock_);
    const auto &node = in[CONFIG_LABEL];

    /* load the device list from the "video" YAML section */
    const auto& devices = node["devices"];
    for (const auto& dev : devices) {
        VideoSettings pref = dev.as<VideoSettings>();
        if (pref.node.empty())
            continue; // discard malformed section
        overwritePreferences(pref);
        auto itd = findDeviceByNode(pref.node);
        if (itd != devices_.end())
            itd->applySettings(pref);
    }

    // Restore the default device if present, or select the first one
    const string pref = preferences_.empty() ? "" : preferences_[0].node;
    const string first = devices_.empty() ? "" : devices_[0].name;
    const auto devIter = findDeviceByNode(pref);
    if (devIter != devices_.end()) {
        defaultDevice_ = devIter->name;
    } else {
        defaultDevice_ = first;
    }
}
#pragma optimize("", on)
}} // namespace jami::video
