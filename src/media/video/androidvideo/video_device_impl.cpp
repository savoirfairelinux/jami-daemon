/*
 *  Copyright (C) 2011-2015 Savoir-faire Linux Inc.
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

namespace ring { namespace video {

/*
 * Array to match Android formats. List formats in ascending
 * preferrence: the format with the lower index will be picked.
 */
struct android_fmt {
    int             code;
    std::string     name;
    enum VideoPixelFormat ring_format;
};

static const struct android_fmt and_formats[] {
    { 17,           "NV21",     VIDEO_PIXFMT_NV21 },
    { 842094169,    "YUV420",   VIDEO_PIXFMT_YUV420P },
};

class VideoDeviceImpl {
    public:
        /**
         * @throw std::runtime_error
         */
        VideoDeviceImpl(const std::string& path);

        std::string name;

        VideoSettings getSettings() const;
        void applySettings(VideoSettings settings);

        DeviceParams getDeviceParams() const;
        std::vector<std::string> getSizeList() const;
        std::vector<unsigned> getRateList() const;
    private:
        void selectFormat();
        std::string getSize(const std::string size) const;
        unsigned getRate(const unsigned rate) const;

        std::vector<int> formats_;
        std::vector<std::string> sizes_;
        std::vector<unsigned> rates_;

        const struct android_fmt *fmt_;
        std::string size_;
        unsigned rate_;
};

void
VideoDeviceImpl::selectFormat()
{
    /*
     * formats_ contains camera parameters as returned by the GetCameraInfo
     * signal, find the matching V4L2 formats
     */
    unsigned int current, best = UINT_MAX;
    for(const auto &fmt : formats_) {
        const struct android_fmt *and_fmt;
        for(and_fmt = std::begin(and_formats);
            and_formats < std::end(and_formats);
            and_fmt++) {
            if (fmt == and_fmt->code) {
                current = and_fmt - std::begin(and_formats);
                break;
            }
        }

        /* No match found, we should add it */
        if (and_fmt == std::end(and_formats)) {
            RING_WARN("AndroidVideo: No format matching %d", fmt);
            continue;
        }

        if (current < best)
            best = current;
    }

    if (best != UINT_MAX) {
        fmt_ = &and_formats[best];
        RING_DBG("AndroidVideo: picked format %s", fmt_->name.c_str());
    }
    else {
        fmt_ = &and_formats[0];
        RING_ERR("AndroidVideo: Could not find a known format to use");
    }
}

VideoDeviceImpl::VideoDeviceImpl(const std::string& path) :
    name(path), formats_(), sizes_(), rates_(), fmt_(nullptr), rate_()
{
    emitSignal<DRing::VideoSignal::GetCameraInfo>(name, &formats_, &sizes_, &rates_);

    selectFormat();
    applySettings(VideoSettings());
}

std::string
VideoDeviceImpl::getSize(const std::string size) const
{
    for (const auto &iter : sizes_) {
        if (iter == size)
            return iter;
    }

    assert(not sizes_.empty());
    return sizes_.back();
}

unsigned
VideoDeviceImpl::getRate(const unsigned rate) const
{
    for (const auto &iter : rates_) {
        if (iter == rate)
            return iter;
    }

    assert(not rates_.empty());
    return rates_.back();
}

std::vector<std::string>
VideoDeviceImpl::getSizeList() const
{
    return sizes_;
}

std::vector<unsigned>
VideoDeviceImpl::getRateList() const
{
    return rates_;
}

void
VideoDeviceImpl::applySettings(VideoSettings settings)
{
    std::stringstream ss;
    int width, height;
    char sep;

    size_ = getSize(settings.video_size);
    rate_ = getRate(settings.framerate);

    ss << size_;
    ss >> width >> sep >> height;

    /* VideoManager will cache these parameters and set them only when device is open */
    emitSignal<DRing::VideoSignal::SetParameters>(name, fmt_->code, width, height, rate_);
}

VideoSettings
VideoDeviceImpl::getSettings() const
{
    VideoSettings settings;
    settings.name = name;
    settings.channel = "default";
    settings.video_size = size_;
    settings.framerate = rate_;
    return settings;
}

DeviceParams
VideoDeviceImpl::getDeviceParams() const
{
    DeviceParams params;
    std::stringstream ss1, ss2;
    char sep;

    ss1 << fmt_->ring_format;
    ss1 >> params.format;

    ss2 << size_;
    ss2 >> params.width >> sep >> params.height;

    params.input = name;
    params.channel =  0;
    params.framerate = rate_;
    return params;
}

VideoDevice::VideoDevice(const std::string& path) :
    deviceImpl_(new VideoDeviceImpl(path))
{
    name = deviceImpl_->name;
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

DeviceParams
VideoDevice::getDeviceParams() const
{
    return deviceImpl_->getDeviceParams();
}

DRing::VideoCapabilities
VideoDevice::getCapabilities() const
{
    DRing::VideoCapabilities cap;

    for (const auto &iter : deviceImpl_->getSizeList()) {
        std::vector<std::string> rates;

        for(const auto &iter : deviceImpl_->getRateList()) {
            std::stringstream ss;
            ss << iter;
            rates.push_back(ss.str());
        }
        cap["default"][iter] = rates;
    }

    return cap;
}

VideoDevice::~VideoDevice()
{}

}} // namespace ring::video
