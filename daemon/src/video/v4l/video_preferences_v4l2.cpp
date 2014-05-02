/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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
#include "../video_preferences.h"
#include "video_device_monitor_impl_v4l.h"

using namespace sfl_video;

VideoPreference::VideoPreference() :
    monitorImpl_(new VideoDeviceMonitorImpl),
    deviceList_(),
    active_(deviceList_.end())
{
    monitorImpl_->start();
}

VideoPreference::~VideoPreference()
{}

/*
 * V4L2 interface.
 */

std::vector<std::string>
VideoPreference::getDeviceList()
{
    return monitorImpl_->getDeviceList();
}

std::vector<std::string>
VideoPreference::getChannelList(const std::string &dev)
{
    return monitorImpl_->getChannelList(dev);
}

std::vector<std::string>
VideoPreference::getSizeList(const std::string &dev, const std::string &channel)
{
    return monitorImpl_->getSizeList(dev, channel);
}

std::vector<std::string>
VideoPreference::getRateList(const std::string &dev, const std::string &channel, const std::string &size)
{
    return monitorImpl_->getRateList(dev, channel, size);
}

/*
 * Interface for a single device.
 */

std::map<std::string, std::string>
VideoPreference::deviceToSettings(const VideoDevice& dev)
{
    std::map<std::string, std::string> settings;

    settings["input"] = monitorImpl_->getDeviceNode(dev.name);

    std::stringstream channel_index;
    channel_index << monitorImpl_->getChannelNum(dev.name, dev.channel);
    settings["channel"] = channel_index.str();

    settings["video_size"] = dev.size;
    size_t x_pos = dev.size.find('x');
    settings["width"] = dev.size.substr(0, x_pos);
    settings["height"] = dev.size.substr(x_pos + 1);

    settings["framerate"] = dev.rate;

    return settings;
}
