/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
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

class OSXVideoSize {
    public:
        OSXVideoSize(const unsigned width, const unsigned height);
        unsigned width;
        unsigned height;
};

class VideoDeviceImpl {
    public:
        /**
         * @throw std::runtime_error
         */
        VideoDeviceImpl(const std::string& path);

        std::string device;
        std::string name;

        std::vector<std::string> getChannelList() const;
        std::vector<std::string> getSizeList(const std::string& channel) const;
        std::vector<std::string> getSizeList() const;
        std::vector<std::string> getRateList(const std::string& channel, const std::string& size) const;
        float getRate(unsigned rate) const;

        VideoSettings getSettings() const;
        void applySettings(VideoSettings settings);

        DeviceParams getDeviceParams() const;

    private:
        const OSXVideoSize extractSize(const std::string &name) const;

        AVCaptureDevice* avDevice_;
        std::vector<OSXVideoSize> available_sizes_;
        OSXVideoSize current_size_;
};

VideoDeviceImpl::VideoDeviceImpl(const std::string& uniqueID)
    : device(uniqueID)
    , current_size_(-1, -1)
    , avDevice_([AVCaptureDevice deviceWithUniqueID:
        [NSString stringWithCString:uniqueID.c_str() encoding:[NSString defaultCStringEncoding]]])
{
    name = [[avDevice_ localizedName] UTF8String];

    for (AVCaptureDeviceFormat* format in avDevice_.formats) {
        std::stringstream ss;
        auto dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
        OSXVideoSize size(dimensions.width, dimensions.height);
        available_sizes_.push_back(size);
    }
    // Set default settings
    applySettings(VideoSettings());
}

OSXVideoSize::OSXVideoSize(const unsigned width, const unsigned height) :
    width(width), height(height) {}

void
VideoDeviceImpl::applySettings(VideoSettings settings)
{
//TODO: add framerate
//    rate_ = size_.getRate(settings["rate"]);
    current_size_ = extractSize(settings.video_size);
}

const OSXVideoSize
VideoDeviceImpl::extractSize(const std::string &name) const
{
    for (const auto item : available_sizes_) {
        std::stringstream ss;
        ss << item.width << "x" << item.height;
        if (ss.str() == name)
            return item;
    }

    // fallback to last size
    if (!available_sizes_.empty()) {
        return available_sizes_.back();
    }
    return OSXVideoSize(-1, -1);
}


DeviceParams
VideoDeviceImpl::getDeviceParams() const
{
    DeviceParams params;
    params.input = "[" + device + "]";
    params.format = "avfoundation";

    params.width = current_size_.width;
    params.height = current_size_.height;

    auto format = [avDevice_ activeFormat];
    auto frameRate = (AVFrameRateRange*)
                    [format.videoSupportedFrameRateRanges objectAtIndex:0];
    params.framerate = frameRate.maxFrameRate;
    return params;
}

VideoSettings
VideoDeviceImpl::getSettings() const
{
    VideoSettings settings;

    settings.name = [[avDevice_ localizedName] UTF8String];

    return settings;
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
VideoDevice::applySettings(VideoSettings settings)
{
    deviceImpl_->applySettings(settings);
}

VideoSettings
VideoDevice::getSettings() const
{
    return deviceImpl_->getSettings();
}

std::vector<std::string>
VideoDeviceImpl::getSizeList() const
{
    return getSizeList("default");
}

std::vector<std::string>
VideoDeviceImpl::getRateList(const std::string& channel, const std::string& size) const
{
    auto format = [avDevice_ activeFormat];
    std::vector<std::string> v;

    for (AVFrameRateRange* frameRateRange in format.videoSupportedFrameRateRanges) {
      std::stringstream ss;
        ss << frameRateRange.maxFrameRate;
        v.push_back(ss.str());
    }
    return v;
}

std::vector<std::string>
VideoDeviceImpl::getSizeList(const std::string& channel) const
{
    std::vector<std::string> v;

    for (const auto &item : available_sizes_) {
        std::stringstream ss;
        ss << item.width << "x" << item.height;
        v.push_back(ss.str());
    }

    return v;
}

std::vector<std::string> VideoDeviceImpl::getChannelList() const
{
    return {"default"};
}

DRing::VideoCapabilities
VideoDevice::getCapabilities() const
{
    DRing::VideoCapabilities cap;

    for (const auto& chan : deviceImpl_->getChannelList())
        for (const auto& size : deviceImpl_->getSizeList(chan))
            cap[chan][size] = deviceImpl_->getRateList(chan, size);

    return cap;
}

VideoDevice::~VideoDevice()
{}

}} // namespace ring::video
