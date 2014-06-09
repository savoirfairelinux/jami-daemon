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

#ifndef VIDEO_PREFERENCE_H__
#define VIDEO_PREFERENCE_H__

#include "config/serializable.h"
#include "noncopyable.h"

#include <map>
#include <string>
#include <memory>

namespace sfl_video {
    class VideoDeviceMonitorImpl;
}

namespace Conf {
    class SequenceNode;
}

typedef std::map<std::string, std::map<std::string, std::vector<std::string>>> VideoCapabilities;

class VideoPreference : public Serializable
{
    public:
        VideoPreference();
        ~VideoPreference();

        /*
         * Video device monitoring specific interface.
         */
        std::vector<std::string> getDeviceList() const;

        VideoCapabilities
        getCapabilities(const std::string& name);

        /*
         * Interface for a single device.
         */
        std::map<std::string, std::string> getSettingsFor(const std::string& name) const;
        std::map<std::string, std::string> getPreferences(const std::string& name) const;
        void setPreferences(const std::string& name, std::map<std::string, std::string> pref);

        /*
         * Interface with the "active" video device.
         * This is the default used device when sending a video stream.
         */
        std::map<std::string, std::string> getSettings() const;

        std::string getDevice() const;
        void setDevice(const std::string& name);

        /*
         * Interface to load from/store to the (YAML) configuration file.
         */
        virtual void serialize(Conf::YamlEmitter &emitter);
        virtual void unserialize(const Conf::YamlNode &map);

    private:
        NON_COPYABLE(VideoPreference);

        struct VideoDevice {
            std::string name = "";
            std::string channel = "";
            std::string size = "";
            std::string rate = "";
        };

        std::unique_ptr<sfl_video::VideoDeviceMonitorImpl> monitorImpl_;

        /*
         * Vector containing the video devices in order of preference
         * (the first is the active one).
         */
        std::string default_ = "";
        std::vector<VideoDevice> deviceList_;

        std::vector<VideoDevice>::iterator lookupDevice(const std::string& name);
        std::vector<VideoDevice>::const_iterator lookupDevice(const std::string& name) const;

        std::vector<std::string> getChannelList(const std::string& name) const;
        std::vector<std::string> getSizeList(const std::string& name, const std::string& channel) const;
        std::vector<std::string> getRateList(const std::string& name, const std::string& channel, const std::string& size) const;

        bool validatePreference(const VideoDevice& dev) const;
        std::map<std::string, std::string> deviceToSettings(const VideoDevice& dev) const;
        static void addDeviceToSequence(const VideoDevice& dev, Conf::SequenceNode& seq);

        VideoDevice defaultPreferences(const std::string& name) const;
        void addDevice(const std::string &name);
};

#endif /* VIDEO_PREFERENCE_H__ */
