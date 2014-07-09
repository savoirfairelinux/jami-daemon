/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <algorithm>
#include <cassert>
#include <sstream>

#include "manager.h"
#include "client/videomanager.h"
#include "config/yamlemitter.h"
#include "config/yamlnode.h"
#include "logger.h"
#include "video_device_monitor.h"

namespace sfl_video {

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

VideoCapabilities
VideoDeviceMonitor::getCapabilities(const string& name) const
{
    const auto iter = findDeviceByName(name);

    if (iter == devices_.end())
        return VideoCapabilities();

    return iter->getCapabilities();
}

VideoSettings
VideoDeviceMonitor::getSettings(const string& name)
{
    const auto itd = findDeviceByName(name);

    if (itd == devices_.end())
        return VideoSettings();

    return itd->getSettings();
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

void
VideoDeviceMonitor::setDefaultDevice(const string& name)
{
    const auto it = findDeviceByName(name);

    if (it != devices_.end())
        defaultDevice_ = it->name;
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
    if (!ManagerImpl::initialized) {
        WARN("Manager not initialized yet");
        return;
    }

    Manager::instance().getVideoManager()->deviceEvent();
}

void
VideoDeviceMonitor::addDevice(const string& node)
{
    if (findDeviceByNode(node) != devices_.end())
        return;

    // instantiate a new unique device
    VideoDevice dev(node);
    giveUniqueName(dev, devices_);

    // restore its preferences if any, or store the defaults
    auto it = findPreferencesByName(dev.name);
    if (it != preferences_.end())
        dev.applySettings(*it);
    else
        preferences_.push_back(dev.getSettings());

    // in case there is no default device on a fresh run
    if (defaultDevice_.empty())
        defaultDevice_ = dev.name;

    devices_.push_back(dev);
    notify();
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
    vector<VideoSettings>::iterator it;

    for (it = preferences_.begin(); it != preferences_.end(); ++it)
        if ((*it)["name"] == name)
            break;

    return it;
}

void
VideoDeviceMonitor::overwritePreferences(VideoSettings settings)
{
    auto it = findPreferencesByName(settings["name"]);
    if (it != preferences_.end())
        preferences_.erase(it);

    preferences_.push_back(settings);
}

void
VideoDeviceMonitor::serialize(Conf::YamlEmitter &emitter)
{
    using namespace Conf;

    // Put the default in first position
    auto def = findPreferencesByName(defaultDevice_);
    if (def != preferences_.end())
        std::iter_swap(preferences_.begin(), def);

    MappingNode devices(nullptr);
    SequenceNode sequence(nullptr);

    for (auto& pref : preferences_) {
        MappingNode *node = new MappingNode(nullptr);

        node->setKeyValue("name", new ScalarNode(pref["name"]));
        node->setKeyValue("channel", new ScalarNode(pref["channel"]));
        node->setKeyValue("size", new ScalarNode(pref["size"]));
        node->setKeyValue("rate", new ScalarNode(pref["rate"]));

        sequence.addNode(node);
    }

    devices.setKeyValue("devices", &sequence);

    /* store the device list under the "video" YAML section */
    emitter.serializePreference(&devices, "video");
}

void
VideoDeviceMonitor::unserialize(const Conf::YamlNode &node)
{
    using namespace Conf;

    /* load the device list from the "video" YAML section */
    YamlNode *devicesNode(node.getValue("devices"));

    if (!devicesNode || devicesNode->getType() != SEQUENCE) {
        ERROR("No 'devices' sequence node! Old config?");
        return;
    }

    SequenceNode *seqNode = static_cast<SequenceNode *>(devicesNode);
    Sequence *seq = seqNode->getSequence();

    if (seq->empty()) {
        WARN("Empty video device list");
        return;
    }

    for (const auto &iter : *seq) {
        MappingNode *devnode = static_cast<MappingNode *>(iter);
        VideoSettings pref;

        devnode->getValue("name", &pref["name"]);
        devnode->getValue("channel", &pref["channel"]);
        devnode->getValue("size", &pref["size"]);
        devnode->getValue("rate", &pref["rate"]);

        overwritePreferences(pref);

        // Restore the device preferences if present
        auto itd = findDeviceByName(pref["name"]);
        if (itd != devices_.end())
            itd->applySettings(pref);
    }

    // Restore the default device if present, or select the first one
    const string pref = preferences_.empty() ? "" : preferences_[0]["name"];
    const string first = devices_.empty() ? "" : devices_[0].name;
    if (findDeviceByName(pref) != devices_.end())
        defaultDevice_ = pref;
    else
        defaultDevice_ = first;
}

} // namespace sfl_video
