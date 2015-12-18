/*
 *  Copyright (C) 2014-2015 Savoir-faire Linux Inc.
 *
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

#ifndef __VIDEO_DEVICE_H__
#define __VIDEO_DEVICE_H__

#include "media/media_device.h"
#include "video_base.h"

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "videomanager_interface.h"
#include "string_utils.h"
#include "logger.h" // for _debug

namespace ring { namespace video {

class VideoDeviceImpl;

class VideoDevice {
public:

    VideoDevice(const std::string& path);
    ~VideoDevice();

    /*
     * The device name, e.g. "Integrated Camera",
     * actually used as the identifier.
     */
    std::string name = "";

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
    DRing::VideoCapabilities getCapabilities() const{
        DRing::VideoCapabilities cap;

        for (const auto& chan : getChannelList())
            for (const auto& size : getSizeList(chan)) {
                std::stringstream sz;
                sz << size.first << "x" << size.second;
                auto rates = getRateList(chan, size);
                std::vector<std::string> rates_str {rates.size()};
                std::transform(rates.begin(), rates.end(), rates_str.begin(), [](rational<double> r) { return ring::to_string(r.real()); });
                cap[chan][sz.str()] = rates_str;
            }

        return cap;
    }

    VideoSettings getDefaultSettings() const {
        VideoSettings settings = getSettings();
        settings.channel = getChannelList().front();

        std::pair<unsigned, unsigned> max_size {0, 0};
        rational<double> max_size_rate {0};

        auto sizes = getSizeList(settings.channel);
        for (auto& s : sizes) {
            if (s.second > 720)
                continue;
            auto rates = getRateList(settings.channel, s);
            if (rates.empty())
                continue;
            auto max_rate = *std::max_element(rates.begin(), rates.end());
            if (max_rate < 10)
                continue;
            if (s.second > max_size.second || (s.second == max_size.second && s.first > max_size.first)) {
                max_size = s;
                max_size_rate = max_rate;
            }
        }
        if (max_size.second > 0) {
            std::stringstream video_size;
            video_size << max_size.first << "x" << max_size.second;
            settings.video_size = video_size.str();
            settings.framerate = ring::to_string(max_size_rate.real());
            RING_WARN("Selecting default: %s, %s FPS", settings.video_size.c_str(), settings.framerate.c_str());
        }

        return settings;
    }

    /*
     * Get the settings for the device.
     */
    VideoSettings getSettings() const;

    /*
     * Setup the device with the preferences listed in the "settings" map.
     * The expected map should be similar to the result of getSettings().
     *
     * If a key is missing, a valid default value is choosen. Thus, calling
     * this function with an empty map will reset the device to default.
     */
    void applySettings(VideoSettings settings);

    /**
     * Returns the parameters needed for actual use of the device
     */
    DeviceParams getDeviceParams() const;

private:

    std::vector<std::string> getChannelList() const;
    std::vector<std::pair<unsigned, unsigned>> getSizeList(const std::string& channel) const;
    std::vector<rational<double>> getRateList(const std::string& channel, std::pair<unsigned, unsigned> size) const;

    /*
     * The device node, e.g. "/dev/video0".
     */
    std::string node_ = "";

    /*
     * Device specific implementation.
     * On Linux, V4L2 stuffs go there.
     *
     * Note: since a VideoDevice is copyable,
     * deviceImpl_ cannot be an unique_ptr.
     */
    std::shared_ptr<VideoDeviceImpl> deviceImpl_;
};

}} // namespace ring::video

#endif // __VIDEO_DEVICE_H__
