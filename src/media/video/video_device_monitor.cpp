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
    vector<string> ids;
    ids.reserve(devices_.size());
    for (const auto& dev : devices_)
        ids.emplace_back(dev.getDeviceId());
    return ids;
}

DRing::VideoCapabilities
VideoDeviceMonitor::getCapabilities(const string& id) const
{
    std::lock_guard<std::mutex> l(lock_);
    const auto iter = findDeviceById(id);
    if (iter == devices_.end())
        return DRing::VideoCapabilities();

    return iter->getCapabilities();
}
#pragma optimize("", off)
VideoSettings
VideoDeviceMonitor::getSettings(const string& id)
{
    std::lock_guard<std::mutex> l(lock_);

    const auto prefIter = findPreferencesById(id);
    if (prefIter == preferences_.end())
        return VideoSettings();

    return *prefIter;
}
#pragma optimize("", on)
void
VideoDeviceMonitor::applySettings(const string& id, const VideoSettings& settings)
{
    std::lock_guard<std::mutex> l(lock_);
    const auto iter = findDeviceById(id);

    if (iter == devices_.end())
        return;

    iter->applySettings(settings);
    auto it = findPreferencesById(settings.id);
    if (it != preferences_.end())
        (*it) = settings;
}

string
VideoDeviceMonitor::getDefaultDevice() const
{
    std::lock_guard<std::mutex> l(lock_);
    const auto it = findDeviceById(defaultDevice_);
    if (it == std::end(devices_))
        return {};
    return it->name;
}
#pragma optimize("", off)
std::string
VideoDeviceMonitor::getMRLForDefaultDevice() const
{
    std::lock_guard<std::mutex> l(lock_);
    const auto it = findDeviceById(defaultDevice_);
    if(it == std::end(devices_))
        return {};
    static const std::string sep = DRing::Media::VideoProtocolPrefix::SEPARATOR;
    return DRing::Media::VideoProtocolPrefix::CAMERA + sep + it->name;
}
#pragma optimize("", on)
void
VideoDeviceMonitor::setDefaultDevice(const std::string& id)
{
    std::lock_guard<std::mutex> l(lock_);
    const auto itDev = findDeviceById(id);
    if (itDev != devices_.end()) {
        defaultDevice_ = itDev->getDeviceId();

        // place it at the begining of the prefs
        auto itPref = findPreferencesById(itDev->getDeviceId());
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
VideoDeviceMonitor::setDeviceOrientation(const std::string& id, int angle)
{
    const auto itd = findDeviceById(id);
    if (itd != devices_.cend()) {
        itd->setOrientation(angle);
    }
}

DeviceParams
VideoDeviceMonitor::getDeviceParams(const std::string& id) const
{
    std::lock_guard<std::mutex> l(lock_);
    const auto itd = findDeviceById(id);
    if (itd == devices_.cend())
        return DeviceParams();
    return itd->getDeviceParams();
}

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
VideoDeviceMonitor::addDevice(const string& id, const std::vector<std::map<std::string, std::string>>* devInfo)
{
    try {
        std::lock_guard<std::mutex> l(lock_);
        if (findDeviceById(id) != devices_.end())
            return;

        // instantiate a new unique device
        VideoDevice dev { id, *devInfo};

        if (dev.getChannelList().empty())
            return;

        giveUniqueName(dev, devices_);

        // restore its preferences if any, or store the defaults
        auto it = findPreferencesById(id);
        if (it != preferences_.end()) {
            dev.applySettings(*it);
        } else {
            dev.applySettings(dev.getDefaultSettings());
            preferences_.emplace_back(dev.getSettings());
        }

        // in case there is no default device on a fresh run
        if (defaultDevice_.empty())
            defaultDevice_ = dev.getDeviceId();

        devices_.emplace_back(std::move(dev));
    } catch (const std::exception& e) {
        JAMI_ERR("Failed to add device %s: %s", id.c_str(), e.what());
        return;
    }
    notify();
}

void
VideoDeviceMonitor::removeDevice(const string& id)
{
    {
        std::lock_guard<std::mutex> l(lock_);
        const auto it = findDeviceById(id);
        if (it == devices_.end())
            return;

        devices_.erase(it);

        if (defaultDevice_.find(id) != std::string::npos) {
            if (devices_.size() == 0) {
                defaultDevice_.clear();
            } else {
                setDefaultDevice(devices_[0].getDeviceId());
            }
        }
    }
    notify();
}

vector<VideoDevice>::iterator
VideoDeviceMonitor::findDeviceById(const string& id)
{
    for (auto it = devices_.begin(); it != devices_.end(); ++it)
        if (it->getDeviceId().find(id) != std::string::npos)
            return it;
    return devices_.end();
}

vector<VideoDevice>::const_iterator
VideoDeviceMonitor::findDeviceById(const string& id) const
{
    for (auto it = devices_.cbegin(); it != devices_.cend(); ++it)
        if (it->getDeviceId().find(id) != std::string::npos)
            return it;
    return devices_.end();
}
#pragma optimize("", off)
vector<VideoSettings>::iterator
VideoDeviceMonitor::findPreferencesById(const string& id)
{
    for (auto it = preferences_.begin(); it != preferences_.end(); ++it)
        if (it->id.find(id) != std::string::npos)
            return it;
    return preferences_.end();
}
#pragma optimize("", on)
void
VideoDeviceMonitor::overwritePreferences(const VideoSettings& settings)
{
    auto it = findPreferencesById(settings.id);
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
        if (pref.id.empty())
            continue; // discard malformed section
        overwritePreferences(pref);
        auto itd = findDeviceById(pref.id);
        if (itd != devices_.end())
            itd->applySettings(pref);
    }

    // Restore the default device if present, or select the first one
    const string prefId = preferences_.empty() ? "" : preferences_[0].id;
    const string firstId = devices_.empty() ? "" : devices_[0].getDeviceId();
    const auto devIter = findDeviceById(prefId);
    if (devIter != devices_.end()) {
        defaultDevice_ = devIter->getDeviceId();
    } else {
        defaultDevice_ = firstId;
    }
}
#pragma optimize("", on)
}} // namespace jami::video
