/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#pragma once

#include "media/media_device.h"
#include "video_base.h"
#include "rational.h"

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

#include "videomanager_interface.h"
#include "string_utils.h"
#include "logger.h"

namespace jami { namespace video {

using VideoSize = std::pair<unsigned, unsigned>;
using FrameRate = rational<double>;

class VideoDeviceImpl;

class VideoDevice {
public:

    VideoDevice(const std::string& path, const std::vector<std::map<std::string, std::string>>& devInfo);
    ~VideoDevice();

    /*
     * The device name, e.g. "Integrated Camera",
     * actually used as the identifier.
     */
    std::string name {};

    const std::string& getNode() const { return node_; }

    /*
     * Get the 3 level deep tree of possible settings for the device.
     * The levels are channels, sizes, and rates.
     *
     * The result map for the "Integrated Camera" looks like this:
     *
     *   {'Camera 1': {'1280x720': ['10'],
     *                 '320x240': ['30', '15'],
     *                 '352x288': ['30', '15'],
     *                 '424x240': ['30', '15'],
     *                 '640x360': ['30', '15'],
     *                 '640x480': ['30', '15'],
     *                 '800x448': ['15'],
     *                 '960x540': ['10']}}
     */
    DRing::VideoCapabilities getCapabilities() const {
        DRing::VideoCapabilities cap;

        for (const auto& chan : getChannelList())
            for (const auto& size : getSizeList(chan)) {
                std::stringstream sz;
                sz << size.first << "x" << size.second;
                auto rates = getRateList(chan, size);
                std::vector<std::string> rates_str {rates.size()};
                std::transform(rates.begin(), rates.end(), rates_str.begin(),
                               [](FrameRate r) { return jami::to_string(r.real()); });
                cap[chan][sz.str()] = std::move(rates_str);
            }

        return cap;
    }

    /* Default setting is found by using following rules:
     * - frame height <= 640 pixels
     * - frame rate >= 10 fps
     */
    VideoSettings getDefaultSettings() const {
        auto settings = getSettings();
        auto channels = getChannelList();
        if (channels.empty())
            return {};
        settings.channel = getChannelList().front();

        VideoSize max_size {0, 0};
        FrameRate max_size_rate {0};

        auto sizes = getSizeList(settings.channel);
        for (auto& s : sizes) {
            if (s.second > 640)
                continue;
            auto rates = getRateList(settings.channel, s);
            if (rates.empty())
                continue;
            auto max_rate = *std::max_element(rates.begin(), rates.end());
            if (max_rate < 10)
                continue;
            if (s.second > max_size.second ||
                (s.second == max_size.second && s.first > max_size.first)) {
                max_size = s;
                max_size_rate = max_rate;
            }
        }
        if (max_size.second > 0) {
            std::stringstream video_size;
            video_size << max_size.first << "x" << max_size.second;
            settings.video_size = video_size.str();
            settings.framerate = jami::to_string(max_size_rate.real());
            JAMI_WARN("Default video settings: %s, %s FPS", settings.video_size.c_str(),
                      settings.framerate.c_str());
        }

        return settings;
    }

    /*
     * Get the settings for the device.
     */
    VideoSettings getSettings() const {
        auto params = getDeviceParams();
        VideoSettings settings;
        settings.name = params.name;
        settings.channel = params.channel_name;
        settings.video_size = sizeToString(params.width, params.height);
        settings.framerate = jami::to_string(params.framerate.real());
        return settings;
    }

    /*
     * Setup the device with the preferences listed in the "settings" map.
     * The expected map should be similar to the result of getSettings().
     *
     * If a key is missing, a valid default value is choosen. Thus, calling
     * this function with an empty map will reset the device to default.
     */
    void applySettings(VideoSettings settings) {
        DeviceParams params {};
        params.name = settings.name;
        params.channel_name = settings.channel;
        auto size = sizeFromString(settings.channel, settings.video_size);
        params.width = size.first;
        params.height = size.second;
        params.framerate = rateFromString(settings.channel, size, settings.framerate);
        setDeviceParams(params);
    }

    void setOrientation(int orientation) {
      orientation_ = orientation;
    }

    /**
     * Returns the parameters needed for actual use of the device
     */
    DeviceParams getDeviceParams() const;
    std::vector<std::string> getChannelList() const;

private:

    std::vector<VideoSize> getSizeList(const std::string& channel) const;
    std::vector<FrameRate> getRateList(const std::string& channel, VideoSize size) const;

    VideoSize sizeFromString(const std::string& channel, const std::string& size) const {
        auto size_list = getSizeList(channel);
        for (const auto& s : size_list) {
            if (sizeToString(s.first, s.second) == size)
                return s;
        }
        return {0, 0};
    }

    std::string sizeToString(unsigned w, unsigned h) const {
        std::stringstream video_size;
        video_size << w << "x" << h;
        return video_size.str();
    }

    FrameRate rateFromString(const std::string& channel, VideoSize size,
                             const std::string& rate) const {
        FrameRate closest {0};
        double rate_val = 0;
        try {
            rate_val = rate.empty() ? 0 : jami::stod(rate);
        } catch (...) {
            JAMI_WARN("Can't read framerate \"%s\"", rate.c_str());
        }
        // fallback to framerate closest to 30 FPS
        if (rate_val == 0)
            rate_val = 30;
        double closest_dist = std::numeric_limits<double>::max();
        auto rate_list = getRateList(channel, size);
        for (const auto& r : rate_list) {
            double dist = std::fabs(r.real() - rate_val);
            if (dist < closest_dist) {
                closest = r;
                closest_dist = dist;
            }
        }
        return closest;
    }

    void setDeviceParams(const DeviceParams&);

    /*
     * The device node, e.g. "/dev/video0".
     */
    std::string node_ {};

    int orientation_ {0};

    /*
     * Device specific implementation.
     * On Linux, V4L2 stuffs go there.
     *
     * Note: since a VideoDevice is copyable,
     * deviceImpl_ cannot be an unique_ptr.
     */
    std::shared_ptr<VideoDeviceImpl> deviceImpl_;
};

}} // namespace jami::video
