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

void
VideoDeviceMonitor::trySettings(const string& name, VideoSettings settings)
{
    const auto iter = findDeviceByName(name);
    if (iter != devices_.end())
        iter->trySettings(settings);
}

VideoSettings
VideoDeviceMonitor::getSettings(const string& name) const
{
    const auto iter = findDeviceByName(name);
    if (iter != devices_.end())
        return iter->getSettings();
    else
        return VideoSettings();
}

VideoCapabilities
VideoDeviceMonitor::getCapabilities(const string& name) const
{
    const auto iter = findDeviceByName(name);
    if (iter != devices_.end())
        return iter->getCapabilities();
    else
        return VideoCapabilities();
}

static int getNumber(const string &name, size_t *sharp)
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

static void giveUniqueName(VideoDevice &dev, const vector<VideoDevice> &devices)
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

void
VideoDeviceMonitor::addDevice(const string &node)
{
    if (findDeviceByNode(node) != devices_.end())
        return;

    VideoDevice dev(node);
    giveUniqueName(dev, devices_);

    for (auto& pref : preferences_) {
        if (pref["name"] == dev.name) {
            dev.trySettings(pref);
            break;
        }
    }

    devices_.push_back(dev);

    // FIXME seems like this cannot be called too soon. right?
    // If so, how to defer this call or check that VideoManager is up?
    if (Manager::instantiated)
        Manager::instance().getVideoManager()->deviceEvent();
}

void
VideoDeviceMonitor::delDevice(const string &node)
{
    const auto it = findDeviceByNode(node);

    if (it == devices_.end())
        return;

    devices_.erase(it);

    // TODO send deviceEvent()?
    //Manager::instance().getVideoManager()->deviceEvent();
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

/*
 * Interface with the "active" video device.
 * This is the default used device when sending a video stream.
 */

string
VideoDeviceMonitor::getDevice() const
{
    if (default_ == nullptr)
        return "";

    return default_->name;
}

void
VideoDeviceMonitor::setDevice(const string& name)
{
    const auto it = findDeviceByName(name);

    if (it != devices_.end())
        default_ = &(*it);
}

void
VideoDeviceMonitor::serialize(Conf::YamlEmitter &emitter)
{
    using namespace Conf;

    vector<VideoSettings> sorted;

    // Store preferences of plugged devices.
    for (const auto& dev : devices_) {
        VideoSettings pref = dev.getSettings();

        // Remove old preferences, if any.
        auto it = preferences_.begin();
        while (it != preferences_.end()) {
            if ((*it)["name"] == pref["name"]) {
                preferences_.erase(it);
                break;
            }
            it++;
        }

        // Put the default device at first position.
        if (&dev == default_)
            sorted.insert(sorted.begin(), pref);
        else
            sorted.push_back(pref);
    }

    // Store the remaining preferences (old or unplugged devices).
    for (const auto& pref : preferences_)
        sorted.push_back(pref);

    preferences_.clear();

    MappingNode devices(nullptr);
    SequenceNode sequence(nullptr);

    for (auto& pref : sorted) {
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
    } else {
        SequenceNode *seqNode = static_cast<SequenceNode *>(devicesNode);
        Sequence *seq = seqNode->getSequence();

        if (seq->empty()) {
            WARN("Empty video device list");
        } else {
            for (const auto &iter : *seq) {
                MappingNode *devnode = static_cast<MappingNode *>(iter);
                VideoSettings pref;

                devnode->getValue("name", &pref["name"]);
                devnode->getValue("channel", &pref["channel"]);
                devnode->getValue("size", &pref["size"]);
                devnode->getValue("rate", &pref["rate"]);

                preferences_.push_back(pref);
            }

            /*
             * The first device in the configuration is the last active one.
             * If it is unplugged, assign the next one.
             */
            for (auto& pref : preferences_) {
                auto it = findDeviceByName(pref["name"]);
                if (it != devices_.end()) {
                    default_ = &(*it);
                    it->trySettings(pref);
                    break;
                }
            }

            // If no preferred device is found, use to the first one (if any).
            if (default_ == nullptr)
                default_ = devices_.data();
        }
    }
}

} // namespace sfl_video
