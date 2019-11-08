/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
 *
 *  Author: Rafaël Carré <rafael.carre@savoirfairelinux.com>
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#include <array>

extern "C" {
#include <libavutil/pixfmt.h>
}

#include "logger.h"
#include "../video_device.h"

#include "client/ring_signal.h"

namespace jami { namespace video {

typedef struct
{
    std::string             name;
    enum AVPixelFormat      pixfmt;
} ios_fmt;

static const std::array<ios_fmt, 4> ios_formats
{
    ios_fmt { "RGBA",       AV_PIX_FMT_RGBA       },
    ios_fmt { "BGRA",       AV_PIX_FMT_BGRA       },
    ios_fmt { "YUV420P",    AV_PIX_FMT_YUV420P    }
};

class VideoDeviceImpl
{
public:
    VideoDeviceImpl(const std::string& path, const std::vector<std::map<std::string, std::string>>& devInfo);

    std::string name;

    DeviceParams getDeviceParams() const;

    void setDeviceParams(const DeviceParams&);
    void selectFormat();

    std::vector<VideoSize> getSizeList() const;
    std::vector<FrameRate> getRateList() const;

private:

    VideoSize getSize(VideoSize size) const;
    FrameRate getRate(FrameRate rate) const;

    std::vector<std::string> formats_ {};
    std::vector<VideoSize> sizes_ {};
    std::vector<FrameRate> rates_ {};

    const ios_fmt* fmt_ {nullptr};
    VideoSize size_ {};
    FrameRate rate_ {};

};

void
VideoDeviceImpl::selectFormat()
{
    unsigned best = UINT_MAX;
    for(auto fmt : formats_) {
        auto f = ios_formats.begin();
        for (; f != ios_formats.end(); ++f) {
            if (f->name == fmt) {
                auto pos = std::distance(ios_formats.begin(), f);
                if (pos < best)
                    best = pos;
                break;
            }
        }
        if (f == ios_formats.end())
            JAMI_WARN("Video: No format matching %s", fmt.c_str());
    }

    if (best != UINT_MAX) {
        fmt_ = &ios_formats[best];
        JAMI_DBG("Video: picked format %s", fmt_->name.c_str());
    }
    else {
        fmt_ = &ios_formats[0];
        JAMI_ERR("Video: Could not find a known format to use");
    }
}

VideoDeviceImpl::VideoDeviceImpl(const std::string& path, const std::vector<std::map<std::string, std::string>>& devInfo)
: name(path)
{
    for (auto& setting : devInfo) {
        formats_.emplace_back(setting.at("format"));
        sizes_.emplace_back(std::stoi(setting.at("width")), std::stoi(setting.at("height")));
        rates_.emplace_back(std::stoi(setting.at("rate")), 1);
    }
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

    ss1 << fmt_->pixfmt;
    ss1 >> params.format;

    params.name = name;
    params.input = name;
    params.channel =  0;
    params.pixel_format = "nv12";
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
    emitSignal<DRing::VideoSignal::ParametersChanged>(name);
}

VideoDevice::VideoDevice(const std::string& path, const std::vector<std::map<std::string, std::string>>& devInfo)
: deviceImpl_(new VideoDeviceImpl(path, devInfo))
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

}} // namespace jami::video

