/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Rafaël Carré <rafael.carre@savoirfairelinux.com>
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
 */

#include "logger.h"
#include "../video_device.h"

#include "client/ring_signal.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>

namespace jami {
namespace video {

/*
 * Array to match Android formats. List formats in ascending
 * preferrence: the format with the lower index will be picked.
 */
struct android_fmt
{
    int code;
    std::string name;
    int ring_format;
};

static const std::array<android_fmt, 2> and_formats {
    android_fmt {17, "NV21", AV_PIX_FMT_NV21},
    android_fmt {842094169, "YUV420", AV_PIX_FMT_YUV420P},
};

class VideoDeviceImpl
{
public:
    /**
     * @throw std::runtime_error
     */
    VideoDeviceImpl(const std::string& path);

    std::string name;

    DeviceParams getDeviceParams() const;
    void setDeviceParams(const DeviceParams&);

    std::vector<VideoSize> getSizeList() const;
    std::vector<FrameRate> getRateList() const;

private:
    void selectFormat();
    VideoSize getSize(const VideoSize& size) const;
    FrameRate getRate(const FrameRate& rate) const;

    std::vector<int> formats_ {};
    std::vector<VideoSize> sizes_ {};
    std::vector<FrameRate> rates_ {};

    const android_fmt* fmt_ {nullptr};
    VideoSize size_ {};
    FrameRate rate_ {};
};

void
VideoDeviceImpl::selectFormat()
{
    /*
     * formats_ contains camera parameters as returned by the GetCameraInfo
     * signal, find the matching V4L2 formats
     */
    unsigned best = UINT_MAX;
    for (auto fmt : formats_) {
        auto f = and_formats.begin();
        for (; f != and_formats.end(); ++f) {
            if (f->code == fmt) {
                auto pos = std::distance(and_formats.begin(), f);
                if (pos < best)
                    best = pos;
                break;
            }
        }
        if (f == and_formats.end())
            JAMI_WARN("AndroidVideo: No format matching %d", fmt);
    }

    if (best != UINT_MAX) {
        fmt_ = &and_formats[best];
        JAMI_DBG("AndroidVideo: picked format %s", fmt_->name.c_str());
    } else {
        fmt_ = &and_formats[0];
        JAMI_ERR("AndroidVideo: Could not find a known format to use");
    }
}

VideoDeviceImpl::VideoDeviceImpl(const std::string& path)
    : name(path)
{
    std::vector<unsigned> sizes;
    std::vector<unsigned> rates;
    formats_.reserve(16);
    sizes.reserve(16);
    rates.reserve(16);
    emitSignal<libjami::VideoSignal::GetCameraInfo>(name, &formats_, &sizes, &rates);
    for (size_t i = 0, n = sizes.size(); i < n; i += 2)
        sizes_.emplace_back(sizes[i], sizes[i + 1]);
    for (const auto& r : rates)
        rates_.emplace_back(r, 1000);

    selectFormat();
}

VideoSize
VideoDeviceImpl::getSize(const VideoSize& size) const
{
    for (const auto& iter : sizes_) {
        if (iter == size)
            return iter;
    }

    return sizes_.empty() ? VideoSize {0, 0} : sizes_.back();
}

FrameRate
VideoDeviceImpl::getRate(const FrameRate& rate) const
{
    for (const auto& iter : rates_) {
        if (iter == rate)
            return iter;
    }

    return rates_.empty() ? FrameRate {} : rates_.back();
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
    params.format = std::to_string(fmt_->ring_format);
    params.unique_id = name;
    params.name = name;
    params.input = name;
    params.channel = 0;
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
    emitSignal<libjami::VideoSignal::SetParameters>(name,
                                                  fmt_->code,
                                                  size_.first,
                                                  size_.second,
                                                  rate_.real());
}

VideoDevice::VideoDevice(const std::string& id,
                         const std::vector<std::map<std::string, std::string>>&)
    : deviceImpl_(new VideoDeviceImpl(id))
{
    id_ = id;
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
VideoDevice::getSizeList(const std::string& /* channel */) const
{
    return deviceImpl_->getSizeList();
}

std::vector<FrameRate>
VideoDevice::getRateList(const std::string& /* channel */, VideoSize /* size */) const
{
    return deviceImpl_->getRateList();
}

VideoDevice::~VideoDevice() {}

} // namespace video
} // namespace jami
