/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
 *
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
#include <array>

#include "logger.h"
#include "../video_device.h"

#include "ring_signal.h"

#include <ciso646>

namespace ring { namespace video {

typedef struct {
    std::string             name;
    enum VideoPixelFormat   ring_format;
} uwp_fmt;

static const std::array<uwp_fmt, 2> uwp_formats {
    uwp_fmt { "NV12",       VIDEO_PIXFMT_BGRA    },
    uwp_fmt { "YUY2",       VIDEO_PIXFMT_BGRA    },
};

class VideoDeviceImpl {
    public:
        VideoDeviceImpl(const std::string& path);

        std::string name;

        DeviceParams getDeviceParams() const;
        void setDeviceParams(const DeviceParams&);

        std::vector<VideoSize> getSizeList() const;
        std::vector<FrameRate> getRateList() const;

    private:

        void selectFormat();
        VideoSize getSize(VideoSize size) const;
        FrameRate getRate(FrameRate rate) const;

        std::vector<std::string> formats_ {};
        std::vector<VideoSize> sizes_ {};
        std::vector<FrameRate> rates_ {};

        const uwp_fmt* fmt_ {nullptr};
        VideoSize size_ {};
        FrameRate rate_ {};

};

void
VideoDeviceImpl::selectFormat()
{
    unsigned best = UINT_MAX;
    for(auto fmt : formats_) {
        auto f = uwp_formats.begin();
        for (; f != uwp_formats.end(); ++f) {
            if (f->name == fmt) {
                auto pos = std::distance(uwp_formats.begin(), f);
                if (pos < best)
                    best = pos;
                break;
            }
        }
        if (f == uwp_formats.end())
            RING_WARN("UWPVideo: No format matching %s", fmt.c_str());
    }

    if (best != UINT_MAX) {
        fmt_ = &uwp_formats[best];
        RING_DBG("UWPVideo: picked format %s", fmt_->name.c_str());
    }
    else {
        fmt_ = &uwp_formats[0];
        RING_ERR("UWPVideo: Could not find a known format to use");
    }
}

VideoDeviceImpl::VideoDeviceImpl(const std::string& path) : name(path)
{
    std::vector<unsigned> sizes;
    std::vector<unsigned> rates;
    formats_.reserve(16);
    sizes.reserve(32);
    rates.reserve(16);
    emitSignal<DRing::VideoSignal::GetCameraInfo>(name, &formats_, &sizes, &rates);
    for (size_t i=0, n=sizes.size(); i<n; i+=2)
        sizes_.emplace_back(sizes[i], sizes[i+1]);
    for (const auto& r : rates)
        rates_.emplace_back(r, 1);

    selectFormat();
}

VideoSize
VideoDeviceImpl::getSize(VideoSize size) const
{
    for (const auto &iter : sizes_) {
        if (iter == size)
            return iter;
    }

    return sizes_.empty() ? VideoSize{0, 0} : sizes_.back();
}

FrameRate
VideoDeviceImpl::getRate(FrameRate rate) const
{
    for (const auto &iter : rates_) {
        if (iter == rate)
            return iter;
    }

    return rates_.empty() ? FrameRate{0, 0} : rates_.back();
}

std::vector<VideoSize>
VideoDeviceImpl::getSizeList() const
{
    return sizes_;
}

std::vector<FrameRate>
VideoDeviceImpl::getRateList() const
{
    return rates_;
}

DeviceParams
VideoDeviceImpl::getDeviceParams() const
{
    DeviceParams params;
    std::stringstream ss1, ss2;

    ss1 << fmt_->ring_format;
    ss1 >> params.format;

    params.name = name;
    params.input = name;
    params.channel =  0;
    params.width = size_.first;
    params.height = size_.second;
    params.framerate = rate_;

    return params;
}

void
VideoDeviceImpl::setDeviceParams(const DeviceParams& params)
{
    size_ = getSize({params.width, params.height});
    rate_ = getRate(params.framerate);
    emitSignal<DRing::VideoSignal::SetParameters>(name, fmt_->name, size_.first, size_.second, rate_.real());
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
    return {"default"};
}

std::vector<VideoSize>
VideoDevice::getSizeList(const std::string& channel) const
{
    return deviceImpl_->getSizeList();
}

std::vector<FrameRate>
VideoDevice::getRateList(const std::string& channel, VideoSize size) const
{
    return deviceImpl_->getRateList();
}

VideoDevice::~VideoDevice()
{}

}} // namespace ring::video
