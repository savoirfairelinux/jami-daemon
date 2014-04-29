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
#include "video_preferences.h"
#include "video_v4l2_list.h"

using namespace sfl_video;

VideoPreference::VideoPreference() :
    v4l2List_(new VideoV4l2ListThread),
    deviceList_(),
    active_(-1)
{
    v4l2List_->start();
}

/*
 * V4L2 interface.
 */

std::vector<std::string>
VideoPreference::getDeviceList()
{
    return v4l2List_->getDeviceList();
}

std::vector<std::string>
VideoPreference::getChannelList(const std::string &dev)
{
    return v4l2List_->getChannelList(dev);
}

std::vector<std::string>
VideoPreference::getSizeList(const std::string &dev, const std::string &channel)
{
    return v4l2List_->getSizeList(dev, channel);
}

std::vector<std::string>
VideoPreference::getRateList(const std::string &dev, const std::string &channel, const std::string &size)
{
    return v4l2List_->getRateList(dev, channel, size);
}

/*
 * Interface for a single device.
 */

void
VideoPreference::addDevice(const std::string &name, bool active)
{
    for (const auto &dev : deviceList_)
        if (dev.name == name)
            return;

    VideoDevice dev;
    dev.name = name;
    if (active)
        active_ = deviceList_.size();
    deviceList_.push_back(dev);
}

VideoPreference::VideoDevice*
VideoPreference::lookupDevice(const std::string& name)
{
    for (auto& dev : deviceList_)
        if (dev.name == name)
            return &dev;
    return nullptr;
}

std::map<std::string, std::string>
VideoPreference::deviceToSettings(const VideoDevice& dev)
{
    std::map<std::string, std::string> settings;

    settings["input"] = v4l2List_->getDeviceNode(dev.name);

    std::stringstream channel_index;
    channel_index << v4l2List_->getChannelNum(dev.name, dev.channel);
    settings["channel"] = channel_index.str();

    settings["video_size"] = dev.size;
    size_t x_pos = dev.size.find('x');
    settings["width"] = dev.size.substr(0, x_pos);
    settings["height"] = dev.size.substr(x_pos + 1);

    settings["framerate"] = dev.rate;

    return settings;
}

std::map<std::string, std::string>
VideoPreference::getSettingsFor(const std::string& name)
{
    std::map<std::string, std::string> settings;

    if (deviceList_.empty())
        return settings;

    auto dev = lookupDevice(name);
    if (dev)
        settings = deviceToSettings(*dev);

    return settings;
}

/*
 * Interface with the "active" video device.
 * This is the default used device when sending a video stream.
 */

std::string
VideoPreference::getDevice() const
{
    if (active_ < 0)
        return "";

    return deviceList_[active_].name;
}

void
VideoPreference::setDevice(const std::string& name)
{
    /*
     * This is actually a hack.
     * v4l2List_ is calling setDevice() when it detects a new device.
     * We addDevice() here until we make V4l2List use it.
     */
    addDevice(name, true);
}

std::string
VideoPreference::getChannel() const
{
    if (active_ < 0)
        return "";

    return deviceList_[active_].channel;
}

void
VideoPreference::setChannel(const std::string& channel)
{
    if (active_ >= 0)
        deviceList_[active_].channel = channel;
}

std::string
VideoPreference::getSize() const
{
    if (active_ < 0)
        return "";

    return deviceList_[active_].size;
}

void
VideoPreference::setSize(const std::string& size)
{
    if (active_ >= 0)
        deviceList_[active_].size = size;
}

std::string
VideoPreference::getRate() const
{
    if (deviceList_.empty())
        return "";

    return deviceList_[active_].rate;
}

void
VideoPreference::setRate(const std::string& rate)
{
    if (active_ >= 0)
        deviceList_[active_].rate = rate;
}


std::map<std::string, std::string>
VideoPreference::getSettings()
{
    std::map<std::string, std::string> settings;

    if (active_ >= 0)
        settings = deviceToSettings(deviceList_[active_]);

    return settings;
}

void
VideoPreference::addDeviceToSequence(const VideoDevice &dev, Conf::SequenceNode &seq)
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
VideoPreference::serialize(Conf::YamlEmitter &emitter)
{
    using namespace Conf;

    MappingNode devices(nullptr);
    SequenceNode sequence(nullptr);

    /* add active device first */
    if (active_ >= 0)
        addDeviceToSequence(deviceList_[active_], sequence);

    int i = 0;
    for (const auto& dev : deviceList_) {
        if (i++ != active_)
            addDeviceToSequence(dev, sequence);
    }

    devices.setKeyValue("devices", &sequence);

    /* store the device list under the "video" YAML section */
    emitter.serializePreference(&devices, "video");
}

void
VideoPreference::unserialize(const Conf::YamlNode &node)
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
            const auto plugged = getDeviceList();
            active_ = -1;
            for (auto iter = deviceList_.begin(); iter != deviceList_.end(); ++iter) {
                if (std::find(plugged.begin(), plugged.end(),
                              (*iter).name) != plugged.end()) {
                    active_ = iter - deviceList_.begin();
                    break;
                }
            }
        }
    }
}
