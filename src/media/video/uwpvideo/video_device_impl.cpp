/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
 *
 *  Author: Edric Milaret <edric.ladent-milaret@savoirfairelinux.com>
 *  Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>
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
#include <climits>
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>

#include "logger.h"
#include "../video_device.h"

#include <ciso646>

namespace ring { namespace video {

class VideoDeviceImpl {
    public:
        /**
        * @throw std::runtime_error
        */
        VideoDeviceImpl(const std::string& path);
        std::string device;
        std::string name;
        unsigned int id;

        std::vector<std::string> getChannelList() const;
        std::vector<VideoSize> getSizeList(const std::string& channel) const;
        std::vector<VideoSize> getSizeList() const;
        std::vector<FrameRate> getRateList(const std::string& channel, VideoSize size) const;

        DeviceParams getDeviceParams() const;
        void setDeviceParams(const DeviceParams&);

    private:

        void setup();
        std::vector<VideoSize> sizeList_;
        std::map<VideoSize, std::vector<FrameRate> > rateList_;

        void fail(const std::string& error);
};

VideoDeviceImpl::VideoDeviceImpl(const std::string& id)
    : id(atoi(id.c_str()))
{
    setup();
}

void
VideoDeviceImpl::setup()
{
    RING_DBG("VideoDeviceImpl::setup");
}

void
VideoDeviceImpl::fail(const std::string& error)
{
    throw std::runtime_error(error);
}

DeviceParams
VideoDeviceImpl::getDeviceParams() const
{
    DeviceParams params;
    return params;
}

void
VideoDeviceImpl::setDeviceParams(const DeviceParams& params)
{
    if (params.width and params.height) {
    }
}

std::vector<VideoSize>
VideoDeviceImpl::getSizeList() const
{
    return sizeList_;
}

std::vector<FrameRate>
VideoDeviceImpl::getRateList(const std::string& channel, VideoSize size) const
{
    (void) channel;
    return rateList_.at(size);
}

std::vector<VideoSize>
VideoDeviceImpl::getSizeList(const std::string& channel) const
{
    (void) channel;
    return sizeList_;
}

std::vector<std::string>
VideoDeviceImpl::getChannelList() const
{
    return {"default"};
}

VideoDevice::VideoDevice(const std::string& path)
    : deviceImpl_(new VideoDeviceImpl(path))
{
    node_ = path;
    name = deviceImpl_->name;
}

DeviceParams
VideoDevice::getDeviceParams() const
{
    return deviceImpl_->getDeviceParams();
}

void
VideoDevice::setDeviceParams(const DeviceParams& params)
{
    return deviceImpl_->setDeviceParams(params);
}

std::vector<std::string>
VideoDevice::getChannelList() const
{
    return deviceImpl_->getChannelList();
}

std::vector<VideoSize>
VideoDevice::getSizeList(const std::string& channel) const
{
    return deviceImpl_->getSizeList(channel);
}

std::vector<FrameRate>
VideoDevice::getRateList(const std::string& channel, VideoSize size) const
{
    return deviceImpl_->getRateList(channel, size);
}

VideoDevice::~VideoDevice()
{}

}} // namespace ring::video
