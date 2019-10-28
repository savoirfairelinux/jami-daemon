/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
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

namespace jami { namespace video {

class VideoDeviceImpl {
    public:
        /**
         * @throw std::runtime_error
         */
        VideoDeviceImpl(const std::string& path);

        std::string id;
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
        FrameRate rate_ {};
        std::map<VideoSize, std::vector<FrameRate>> available_rates_;
};

VideoDeviceImpl::VideoDeviceImpl(const std::string& uniqueID)
    : id(uniqueID)
    , current_size_(-1, -1)
    , avDevice_([AVCaptureDevice deviceWithUniqueID:
        [NSString stringWithCString:uniqueID.c_str() encoding:[NSString defaultCStringEncoding]]])
{
    name = [[avDevice_ localizedName] UTF8String];

    available_sizes_.reserve(avDevice_.formats.count);
    for (AVCaptureDeviceFormat* format in avDevice_.formats) {
        auto dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
        available_sizes_.emplace_back(dimensions.width, dimensions.height);
        std::vector<FrameRate> v;
        v.reserve(format.videoSupportedFrameRateRanges.count);
        for (AVFrameRateRange* frameRateRange in format.videoSupportedFrameRateRanges) {
            if(std::find(v.begin(), v.end(), frameRateRange.maxFrameRate) == v.end()) {
                v.emplace_back(frameRateRange.maxFrameRate);
            }
        }
        // if we have multiple formats with the same resolution use video supported framerates from last one
        // because this format will be selected by ffmpeg
        if (available_rates_.find( VideoSize(dimensions.width, dimensions.height) ) == available_rates_.end()) {
            available_rates_.emplace(VideoSize(dimensions.width, dimensions.height), v);
        } else {
            available_rates_.at(VideoSize(dimensions.width, dimensions.height)) = v;
        }
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
    params.input = id;
    params.framerate = rate_;
    params.format = "avfoundation";
    params.pixel_format = "nv12";
    params.width = current_size_.first;
    params.height = current_size_.second;
    return params;
}

void
VideoDeviceImpl::setDeviceParams(const DeviceParams& params)
{
    rate_ = params.framerate;
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
    return available_rates_.at(size);
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

VideoDevice::VideoDevice(const std::string& path, const std::vector<std::map<std::string, std::string>>&) :
    deviceImpl_(new VideoDeviceImpl(path))
{
    id_ = path;
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

}} // namespace jami::video
