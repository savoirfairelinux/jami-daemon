/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "video_preferences.h"
#include "video_v4l2_list.h"
#include "logger.h"
#include "config/yamlnode.h"
#include "config/yamlemitter.h"
#include <sstream>

using namespace sfl_video;

VideoPreference::VideoPreference() :
    v4l2_list_(new VideoV4l2ListThread), device_(), channel_(), size_(), rate_()
{
    v4l2_list_->start();
}

std::map<std::string, std::string> VideoPreference::getSettingsFor(const std::string& device)
{
    std::map<std::string, std::string> args;
    if (not device.empty()) {
        args["input"] = v4l2_list_->getDeviceNode(device);
        std::stringstream ss;
        ss << v4l2_list_->getChannelNum(device, channel_);
        args["channel"] = ss.str();
        args["video_size"] = size_;
        size_t x_pos = size_.find("x");
        args["width"] = size_.substr(0, x_pos);
        args["height"] = size_.substr(x_pos + 1);
        args["framerate"] = rate_;
    }

    return args;
}

std::map<std::string, std::string> VideoPreference::getSettings()
{
	return getSettingsFor(device_);
}

void VideoPreference::serialize(Conf::YamlEmitter &emitter)
{
    Conf::MappingNode preferencemap(NULL);

    Conf::ScalarNode device(device_);
    Conf::ScalarNode channel(channel_);
    Conf::ScalarNode size(size_);
    Conf::ScalarNode rate(rate_);

    preferencemap.setKeyValue(videoDeviceKey, &device);
    preferencemap.setKeyValue(videoChannelKey, &channel);
    preferencemap.setKeyValue(videoSizeKey, &size);
    preferencemap.setKeyValue(videoRateKey, &rate);

    emitter.serializePreference(&preferencemap, "video");
}

void VideoPreference::unserialize(const Conf::YamlNode &map)
{
    map.getValue(videoDeviceKey, &device_);
    map.getValue(videoChannelKey, &channel_);
    map.getValue(videoSizeKey, &size_);
    map.getValue(videoRateKey, &rate_);
}

std::vector<std::string>
VideoPreference::getDeviceList() {
    return v4l2_list_->getDeviceList();
}

std::vector<std::string>
VideoPreference::getChannelList(const std::string &dev) {
    return v4l2_list_->getChannelList(dev);
}

std::vector<std::string>
VideoPreference::getSizeList(const std::string &dev, const std::string &channel) {
    return v4l2_list_->getSizeList(dev, channel);
}

std::vector<std::string>
VideoPreference::getRateList(const std::string &dev, const std::string &channel, const std::string &size) {
    return v4l2_list_->getRateList(dev, channel, size);
}
