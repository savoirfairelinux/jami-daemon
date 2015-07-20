/*
 *  Copyright (C) 2011-2015 Savoir-Faire Linux Inc.
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

#include "logger.h"
#include "../video_device.h"

#include "client/ring_signal.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <linux/videodev2.h>

namespace ring { namespace video {

/*
 * Array to match Android formats to V4L2. List formats in ascending
 * preferrence: the format with the lower index will be picked.
 */
static const int and_v4l2_fmts[][2] {
    { 17,           V4L2_PIX_FMT_NV21 },
    { 842094169,    V4L2_PIX_FMT_YUV420 },
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

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

        int android_format_;
        int v4l2_format_;
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
    for(const auto &iter : formats_) {
        unsigned int i;
        for(i = 0; i < ARRAY_SIZE(and_v4l2_fmts); i++) {
            if (iter == and_v4l2_fmts[i][0]) {
                current = i;
                break;
            }
        }

        /* No match found, we should add it */
        if (i == ARRAY_SIZE(and_v4l2_fmts)) {
            RING_WARN("AndroidVideo: No format matching %d", iter);
            continue;
        }

        if (current < best)
            best = current;
    }

    if (best != UINT_MAX) {
        android_format_ = and_v4l2_fmts[best][0];
        v4l2_format_ = and_v4l2_fmts[best][1];
    }
}

VideoDeviceImpl::VideoDeviceImpl(const std::string& path) :
    name(path), formats_(), sizes_(), rates_(), android_format_(), v4l2_format_(), rate_()
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
    size_ = getSize(settings.video_size);
    rate_ = getRate(settings.framerate);
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
    params.input = name;
    params.format = "android";
    params.channel =  0;
    sscanf(size_.c_str(), "%dx%d", &params.width, &params.height);
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
