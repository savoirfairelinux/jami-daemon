/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef VIDEO_DEVICE_MONITOR_H__
#define VIDEO_DEVICE_MONITOR_H__

#include "config/serializable.h"
#include "noncopyable.h"

#include <map>
#include <string>
#include <memory>

#include "video_device.h"

namespace Conf {
    class SequenceNode;
}

namespace sfl_video {

class VideoDeviceMonitorImpl;

class VideoDeviceMonitor : public Serializable
{
    public:
        VideoDeviceMonitor();
        ~VideoDeviceMonitor();

        std::vector<std::string> getDeviceList() const;

        VideoCapabilities getCapabilities(const std::string& name) const;
        VideoSettings getSettings(const std::string& name);
        void applySettings(const std::string& name, VideoSettings settings);

        std::string getDefaultDevice() const;
        void setDefaultDevice(const std::string& name);

        void addDevice(const std::string &node);
        void removeDevice(const std::string &node);

        /*
         * Interface to load from/store to the (YAML) configuration file.
         */
        virtual void serialize(Conf::YamlEmitter &emitter);
        virtual void unserialize(const Conf::YamlNode &map);

    private:
        NON_COPYABLE(VideoDeviceMonitor);

        /*
         * User preferred settings for a device,
         * as loaded from (and stored to) the configuration file.
         */
        std::vector<VideoSettings> preferences_;

        void overwritePreferences(VideoSettings settings);
        std::vector<VideoSettings>::iterator findPreferencesByName(const std::string& name);

        /*
         * Vector containing the video devices.
         */
        std::vector<VideoDevice> devices_;
        std::string defaultDevice_ = "";

        std::vector<VideoDevice>::iterator findDeviceByName(const std::string& name);
        std::vector<VideoDevice>::const_iterator findDeviceByName(const std::string& name) const;
        std::vector<VideoDevice>::iterator findDeviceByNode(const std::string& node);
        std::vector<VideoDevice>::const_iterator findDeviceByNode(const std::string& node) const;

        std::unique_ptr<VideoDeviceMonitorImpl> monitorImpl_;
};

} // namespace sfl_video

#endif /* VIDEO_DEVICE_MONITOR_H__ */
