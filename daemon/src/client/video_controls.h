/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
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

#ifndef VIDEO_CONTROLS_H_
#define VIDEO_CONTROLS_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_DBUS
#include "dbus/dbus_cpp.h"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbus/video_controls-glue.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

#endif // HAVE_DBUS

#include <memory> // for shared_ptr
#include "video/video_preferences.h"
#include "video/video_base.h"
#include "video/video_input_selector.h"

class VideoControls : public org::sflphone::SFLphone::VideoControls_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor {
    private:
        std::shared_ptr<sfl_video::VideoInputSelector> videoInputSelector_;
        VideoPreference videoPreference_;
        // Only modified from main thread
        int inputClients_; // XXX necessary with the videoInputSelector_?

    public:

        VideoControls(DBus::Connection& connection);
        VideoPreference &getVideoPreferences();

        std::vector<std::map<std::string, std::string> >
        getCodecs(const std::string& accountID);

        void
        setCodecs(const std::string& accountID,
                  const std::vector<std::map<std::string, std::string> > &details);

        std::vector<std::string>
        getDeviceList();

        std::vector<std::string>
        getDeviceChannelList(const std::string &dev);

        std::vector<std::string>
        getDeviceSizeList(const std::string &dev, const std::string &channel);

        std::vector<std::string>
        getDeviceRateList(const std::string &dev, const std::string &channel, const std::string &size);

        std::map<std::string, std::string>
        getSettingsFor(const std::string& device);

        std::map<std::string, std::string>
        getSettings();

        void
        setActiveDevice(const std::string &dev);

        void
        setActiveDeviceChannel(const std::string &channel);

        void
        setActiveDeviceSize(const std::string &size);

        void
        setActiveDeviceRate(const std::string &rate);

        std::string
        getActiveDevice();

        std::string
        getActiveDeviceChannel();

        std::string
        getActiveDeviceSize();

        std::string
        getActiveDeviceRate();

        std::string
        getCurrentCodecName(const std::string &callID);

        void startCamera();
        void stopCamera();
        bool switchInput(const std::string& resource);
        bool hasCameraStarted();
        std::weak_ptr<sfl_video::VideoFrameActiveWriter> getVideoCamera();
};

#endif // VIDEO_CONTROLS_H_
