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

namespace ring { namespace video {

class VideoAndroidSize {
    public:
        VideoAndroidSize(std::string size, std::string camera_id, int format);

        std::string str() const;
        float getRate(std::string) const;
        float getRate(unsigned) const;
        std::vector<std::string> getRateList() const;

        unsigned width_;
        unsigned height_;
    private:
        std::vector<float> rates_;
};

class VideoDeviceImpl {
    public:
        /**
         * @throw std::runtime_error
         */
        VideoDeviceImpl(const std::string& path);

        std::string camera_id;
        std::string name;

        const VideoAndroidSize& getSize(const std::string size) const;

        VideoSettings getSettings() const;
        void applySettings(VideoSettings settings);

        DeviceParams getDeviceParams() const;
        std::vector<std::string> getSizeList() const;
    private:
        int format_;
        std::string size_;
        unsigned rate_;
        std::vector<VideoAndroidSize> sizes_;
};

/* VideoAndroidSize */
VideoAndroidSize::VideoAndroidSize(std::string size, std::string camera_id, int format)
{
    sscanf(size.c_str(), "%ux%u", &width_, &height_);

    emitSignal<DRing::VideoSignal::GetCameraRates>(camera_id, format, size, &rates_);
}

std::string VideoAndroidSize::str() const
{
    std::stringstream ss;
    ss << width_ << "x" << height_;
    return ss.str();
}

float VideoAndroidSize::getRate(unsigned rate) const
{
    for(const auto r : rates_) {
        if (r == rate)
            return r;
    }

    assert(not rates_.empty());
    return rates_.back();
}

float VideoAndroidSize::getRate(std::string rate) const
{
    unsigned r;
    std::stringstream ss;
    ss << rate;
    ss >> r;

    return getRate(r);
}

std::vector<std::string> VideoAndroidSize::getRateList() const
{
    std::vector<std::string> v;

    for(const auto &rate : rates_) {
        std::stringstream ss;
        ss << rate;
        v.push_back(ss.str());
    }

    return v;
}

/* VideoDeviceImpl */
VideoDeviceImpl::VideoDeviceImpl(const std::string& path) :
    camera_id(path), name(path), format_(), rate_()
{
    emitSignal<DRing::VideoSignal::AcquireCamera>(camera_id);
    RING_DBG("### Acquired Camera %s", camera_id.c_str());

    std::vector<int> formats;
    emitSignal<DRing::VideoSignal::GetCameraFormats>(camera_id, &formats);

    assert(not formats.empty());
    format_ = formats[0]; /* FIXME: select a real value */

    std::vector<std::string> sizes;
    emitSignal<DRing::VideoSignal::GetCameraSizes>(camera_id, format_, &sizes);
    for(const auto &iter : sizes) {
        sizes_.push_back(VideoAndroidSize(iter, camera_id, format_));
    }

    emitSignal<DRing::VideoSignal::ReleaseCamera>(camera_id);

    // Set default settings
    applySettings(VideoSettings());
}

const VideoAndroidSize& VideoDeviceImpl::getSize(const std::string size) const
{
    for (const auto &iter : sizes_) {
        if (iter.str() == size)
            return iter;
    }

    assert(not sizes_.empty());
    return sizes_.back();
}

std::vector<std::string> VideoDeviceImpl::getSizeList() const
{
    std::vector<std::string> v;

    for(const auto &iter : sizes_)
        v.push_back(iter.str());

    return v;
}

void
VideoDeviceImpl::applySettings(VideoSettings settings)
{
    const VideoAndroidSize &s = getSize(settings.video_size);
    size_ = s.str();
    rate_ = s.getRate(settings.framerate);
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
    const VideoAndroidSize &s = getSize(size_);

    DeviceParams params;
    params.input = camera_id;
    params.format = "android";
    params.channel =  0;
    params.width = s.width_;
    params.height = s.height_;
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
        const VideoAndroidSize &s = deviceImpl_->getSize(iter);
        cap["default"][s.str()] = s.getRateList();
    }

    return cap;
}

VideoDevice::~VideoDevice()
{}

}} // namespace ring::video
