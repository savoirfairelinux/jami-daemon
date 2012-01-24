/*
 *  Copyright (C) 2012 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef VIDEO_CONTROLS_H_
#define VIDEO_CONTROLS_H_

#include "dbus_cpp.h"

#include <tr1/memory> // for shared_ptr

namespace sfl_video {
    class VideoPreview;
}

class VideoControls
    : public org::sflphone::SFLphone::ConfigurationManager_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor {
    private:
        std::tr1::shared_ptr<sfl_video::VideoPreview> preview_;
        VideoPreference videoPreference;

    public:

        VideoControls(DBus::Connection& connection);
        static const char* SERVER_PATH;

        std::vector< std::string > getVideoCodecList();
        std::vector< std::string > getVideoCodecDetails(const std::string& payload);
        std::vector<std::string> getActiveVideoCodecList(const std::string& accountID);
        void setActiveVideoCodecList(const std::vector<std::string>& list, const std::string& accountID);

        std::vector<std::string> getVideoInputDeviceList();
        std::vector<std::string> getVideoInputDeviceChannelList(const std::string &dev);
        std::vector<std::string> getVideoInputDeviceSizeList(const std::string &dev, const std::string &channel);
        std::vector<std::string> getVideoInputDeviceRateList(const std::string &dev, const std::string &channel, const std::string &size);
        void setVideoInputDevice(const std::string& api);
        void setVideoInputDeviceChannel(const std::string& api);
        void setVideoInputDeviceSize(const std::string& api);
        void setVideoInputDeviceRate(const std::string& api);
        std::string getVideoInputDevice();
        std::string getVideoInputDeviceChannel();
        std::string getVideoInputDeviceSize();
        std::string getVideoInputDeviceRate();

        void startVideoPreview(int32_t &width, int32_t &height, int32_t &shmKey, int32_t &semKey, int32_t &videoBufferSize);
        void stopVideoPreview();
        std::string getCurrentVideoCodecName(const std::string& callID);
};

#endif // VIDEO_CONTROLS_H_

