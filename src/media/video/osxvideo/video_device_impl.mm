/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
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

#include "logger.h"
#include "../video_device.h"

#import <AVFoundation/AVFoundation.h>

namespace ring { namespace video {

class VideoDeviceImpl {
    public:
        /**
         * @throw std::runtime_error
         */
        VideoDeviceImpl(const std::string& path);

        std::string device;
        std::string name;

        std::vector<std::string> getChannelList() const;
        std::vector<VideoSize> getSizeList(const std::string& channel) const;
        std::vector<VideoSize> getSizeList() const;
        std::vector<FrameRate> getRateList(const std::string& channel, VideoSize size) const;

        DeviceParams getDeviceParams() const;
        void setDeviceParams(const DeviceParams&);

    private:
        VideoSize extractSize(VideoSize) const;

        AVCaptureDevice* avDevice_;
        std::vector<VideoSize> available_sizes_;
        VideoSize current_size_;
};

VideoDeviceImpl::VideoDeviceImpl(const std::string& uniqueID)
    : device(uniqueID)
    , current_size_(-1, -1)
    , avDevice_([AVCaptureDevice deviceWithUniqueID:
        [NSString stringWithCString:uniqueID.c_str() encoding:[NSString defaultCStringEncoding]]])
{
    name = [[avDevice_ localizedName] UTF8String];

    available_sizes_.reserve(avDevice_.formats.count);
    for (AVCaptureDeviceFormat* format in avDevice_.formats) {
        auto dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
        available_sizes_.emplace_back(dimensions.width, dimensions.height);
    }
}

VideoSize
VideoDeviceImpl::extractSize(VideoSize size) const
{
    for (const auto item : available_sizes_) {
        if (item.first == size.first && item.second == size.second)
            return item;
    }

    // fallback to last size
    if (!available_sizes_.empty()) {
        return available_sizes_.back();
    }
    return VideoSize(0, 0);
}

DeviceParams
VideoDeviceImpl::getDeviceParams() const
{
    DeviceParams params;
    params.name = [[avDevice_ localizedName] UTF8String];
    params.input = "[" + device + "]";
    params.format = "avfoundation";

    params.width = current_size_.first;
    params.height = current_size_.second;

    auto format = [avDevice_ activeFormat];
    auto frameRate = (AVFrameRateRange*)
                    [format.videoSupportedFrameRateRanges objectAtIndex:0];
    params.framerate = frameRate.maxFrameRate;
    return params;
}

void
VideoDeviceImpl::setDeviceParams(const DeviceParams& params)
{
//TODO: add framerate
//    rate_ = size_.getRate(settings["rate"]);
    current_size_ = extractSize({params.width, params.height});
}

std::vector<VideoSize>
VideoDeviceImpl::getSizeList() const
{
    return getSizeList("default");
}

std::vector<FrameRate>
VideoDeviceImpl::getRateList(const std::string& channel, VideoSize size) const
{
    auto format = [avDevice_ activeFormat];
    std::vector<FrameRate> v;
    v.reserve(format.videoSupportedFrameRateRanges.count);
    for (AVFrameRateRange* frameRateRange in format.videoSupportedFrameRateRanges)
        v.emplace_back(frameRateRange.maxFrameRate);
    return v;
}

std::vector<VideoSize>
VideoDeviceImpl::getSizeList(const std::string& channel) const
{
    return available_sizes_;
}

std::vector<std::string>
VideoDeviceImpl::getChannelList() const
{
    return {"default"};
}

VideoDevice::VideoDevice(const std::string& path) :
    deviceImpl_(new VideoDeviceImpl(path))
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
