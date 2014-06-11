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
#include <sstream>

#include "config/yamlemitter.h"
#include "config/yamlnode.h"
#include "logger.h"
#include "video_device_monitor.h"

using namespace sfl_video;

/*
 * Interface for a single device.
 */

VideoCapabilities
VideoDeviceMonitor::getCapabilities(const std::string& name) const
{
    VideoCapabilities cap;

    for (const auto& chan : getChannelList(name))
        for (const auto& size : getSizeList(name, chan))
            cap[chan][size] = getRateList(name, chan, size);

    return cap;
}

VideoDeviceMonitor::VideoDevice
VideoDeviceMonitor::defaultPreferences(const std::string& name) const
{
    VideoDevice dev;
    dev.name = name;

    auto list = getChannelList(dev.name);
    dev.channel = list.empty() ? "" : list[0];

    list = getSizeList(dev.name, dev.channel);
    dev.size = list.empty() ? "" : list[0];

    list = getRateList(dev.name, dev.channel, dev.size);
    dev.rate = list.empty() ? "" : list[0];

    return dev;
}

void
VideoDeviceMonitor::addDevice(const std::string &name)
{
    for (const auto &dev : deviceList_)
        if (dev.name == name)
            return;

    VideoDevice dev = defaultPreferences(name);
    deviceList_.push_back(dev);
}

std::vector<VideoDeviceMonitor::VideoDevice>::iterator
VideoDeviceMonitor::lookupDevice(const std::string& name)
{
    // Find the device in the cache of preferences
    std::vector<VideoDeviceMonitor::VideoDevice>::iterator it;
    for (it = deviceList_.begin(); it != deviceList_.end(); ++it)
        if (it->name == name)
            break;

    // Check if the device is detected
    if (it != deviceList_.end())
        for (const auto& plugged : getDeviceList())
            if (plugged == it->name)
                return it;

    // Device not found
    return deviceList_.end();
}


std::vector<VideoDeviceMonitor::VideoDevice>::const_iterator
VideoDeviceMonitor::lookupDevice(const std::string& name) const
{
    // Find the device in the cache of preferences
    std::vector<VideoDeviceMonitor::VideoDevice>::const_iterator it;
    for (it = deviceList_.begin(); it != deviceList_.end(); ++it)
        if (it->name == name)
            break;

    // Check if the device is detected
    if (it != deviceList_.end())
        for (const auto& plugged : getDeviceList())
            if (plugged == it->name)
                return it;

    // Device not found
    return deviceList_.end();
}

std::map<std::string, std::string>
VideoDeviceMonitor::getSettingsFor(const std::string& name) const
{
    std::map<std::string, std::string> settings;

    const auto iter = lookupDevice(name);

    if (iter != deviceList_.end())
        settings = deviceToSettings(*iter);

    return settings;
}

std::map<std::string, std::string>
VideoDeviceMonitor::getPreferences(const std::string& name) const
{
    std::map<std::string, std::string> pref;
    const auto it = lookupDevice(name);

    if (it != deviceList_.end()) {
        pref["name"] = it->name;
        pref["channel"] = it->channel;
        pref["size"] = it->size;
        pref["rate"] = it->rate;
    }

    return pref;
}

bool
VideoDeviceMonitor::validatePreference(const VideoDevice& dev) const
{
    DEBUG("prefs: name:%s channel:%s size:%s rate:%s", dev.name.data(),
            dev.channel.data(), dev.size.data(), dev.rate.data());

    // Validate the channel
    const auto chans = getChannelList(dev.name);
    if (std::find(chans.begin(), chans.end(), dev.channel) == chans.end()) {
        DEBUG("Bad channel, ignoring");
        return false;
    }

    // Validate the size
    const auto sizes = getSizeList(dev.name, dev.channel);
    if (std::find(sizes.begin(), sizes.end(), dev.size) == sizes.end()) {
        DEBUG("Bad size, ignoring");
        return false;
    }

    // Validate the rate
    const auto rates = getRateList(dev.name, dev.channel, dev.size);
    if (std::find(rates.begin(), rates.end(), dev.rate) == rates.end()) {
        DEBUG("Bad rate, ignoring");
        return false;
    }

    return true;
}

void
VideoDeviceMonitor::setPreferences(const std::string& name,
        std::map<std::string, std::string> pref)
{
    // Validate the name
    const auto it = lookupDevice(name);
    if (it == deviceList_.end()) {
        DEBUG("Device not found, ignoring");
        return;
    }

    VideoDevice dev;
    dev.name = name;
    dev.channel = pref["channel"];
    dev.size = pref["size"];
    dev.rate = pref["rate"];

    if (!validatePreference(dev)) {
        ERROR("invalid settings");
        return;
    }

    it->channel = dev.channel;
    it->size = dev.size;
    it->rate = dev.rate;
}

/*
 * Interface with the "active" video device.
 * This is the default used device when sending a video stream.
 */

std::string
VideoDeviceMonitor::getDevice() const
{
    // Default device not set or not detected?
    if (lookupDevice(default_) == deviceList_.end())
        return "";

    return default_;
}

void
VideoDeviceMonitor::setDevice(const std::string& name)
{

    /*
     * This is actually a hack.
     * v4l2List_ is calling setDevice() when it detects a new device.
     * We addDevice() here until we make V4l2List use it.
     */
    addDevice(name);

    if (lookupDevice(name) != deviceList_.end())
        default_ = name;
}

std::map<std::string, std::string>
VideoDeviceMonitor::getSettings() const
{
    std::map<std::string, std::string> settings;

    const auto it = lookupDevice(default_);
    if (it != deviceList_.end() && validatePreference(*it))
        settings = deviceToSettings(*it);

    return settings;
}

void
VideoDeviceMonitor::addDeviceToSequence(const VideoDevice& dev,
        Conf::SequenceNode& seq)
{
    using namespace Conf;

    MappingNode *node = new MappingNode(nullptr);

    node->setKeyValue("name", new ScalarNode(dev.name));
    node->setKeyValue("channel", new ScalarNode(dev.channel));
    node->setKeyValue("size", new ScalarNode(dev.size));
    node->setKeyValue("rate", new ScalarNode(dev.rate));

    seq.addNode(node);
}

void
VideoDeviceMonitor::serialize(Conf::YamlEmitter &emitter)
{
    using namespace Conf;

    MappingNode devices(nullptr);
    SequenceNode sequence(nullptr);

    if (!deviceList_.empty()) {
        /* add active device first */
        auto it = lookupDevice(default_);
        if (it != deviceList_.end())
            addDeviceToSequence(*it, sequence);

        for (const auto& dev : deviceList_)
            if (dev.name != default_)
                addDeviceToSequence(dev, sequence);
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
                VideoDevice device;

                devnode->getValue("name", &device.name);
                devnode->getValue("channel", &device.channel);
                devnode->getValue("size", &device.size);
                devnode->getValue("rate", &device.rate);

                deviceList_.push_back(device);
            }

            /*
             * The first device in the configuration is the last active one.
             * If it is unplugged, assign the next one.
             */
            for (const auto& pref : deviceList_) {
                if (lookupDevice(pref.name) != deviceList_.end()) {
                    default_ = pref.name;
                    break;
                }
            }
        }
    }
}
