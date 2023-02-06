/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef VIDEO_DEVICE_MONITOR_H__
#define VIDEO_DEVICE_MONITOR_H__

#include "config/serializable.h"
#include "noncopyable.h"

#include <map>
#include <string>
#include <memory>

#include "video_device.h"

namespace YAML {
class Emitter;
class Node;
} // namespace YAML

namespace jami {
namespace video {

class VideoDeviceMonitorImpl;

class VideoDeviceMonitor : public Serializable
{
public:
    VideoDeviceMonitor();
    ~VideoDeviceMonitor();

    std::vector<std::string> getDeviceList() const;

    libjami::VideoCapabilities getCapabilities(const std::string& name) const;
    VideoSettings getSettings(const std::string& name);
    void applySettings(const std::string& name, const VideoSettings& settings);

    std::string getDefaultDevice() const;
    std::string getMRLForDefaultDevice() const;
    bool setDefaultDevice(const std::string& name);
    void setDeviceOrientation(const std::string& id, int angle);

    bool addDevice(const std::string& node,
                   const std::vector<std::map<std::string, std::string>>& devInfo = {});
    void removeDevice(const std::string& node);
    void removeDeviceViaInput(const std::string& path);

    /**
     * Params for libav
     */
    DeviceParams getDeviceParams(const std::string& name) const;

    /*
     * Interface to load from/store to the (YAML) configuration file.
     */
    void serialize(YAML::Emitter& out) const override;
    virtual void unserialize(const YAML::Node& in) override;

private:
    NON_COPYABLE(VideoDeviceMonitor);

    mutable std::mutex lock_;
    /*
     * User preferred settings for a device,
     * as loaded from (and stored to) the configuration file.
     */
    std::vector<VideoSettings> preferences_;

    void overwritePreferences(const VideoSettings& settings);
    std::vector<VideoSettings>::iterator findPreferencesById(const std::string& id);

    /*
     * Vector containing the video devices.
     */
    std::vector<VideoDevice> devices_;
    std::string defaultDevice_ = "";

    std::vector<VideoDevice>::iterator findDeviceById(const std::string& id);
    std::vector<VideoDevice>::const_iterator findDeviceById(const std::string& id) const;

    std::unique_ptr<VideoDeviceMonitorImpl> monitorImpl_;

    constexpr static const char* CONFIG_LABEL = "video";
};

} // namespace video
} // namespace jami

#endif /* VIDEO_DEVICE_MONITOR_H__ */
