/*
 *  Copyright (C) 2012-2014 Savoir-Faire Linux Inc.
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

#ifndef VIDEOMANAGER_H_
#define VIDEOMANAGER_H_

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
#include "dbus/videomanager-glue.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

#endif // HAVE_DBUS

#include <memory> // for weak/shared_ptr
#include "video/video_device_monitor.h"
#include "video/video_base.h"
#include "video/video_input.h"

class VideoManager
#if HAVE_DBUS
    : public org::sflphone::SFLphone::VideoManager_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor
#endif
{
    private:
        /* VideoManager acts as a cache of the active VideoInput.
         * When this input is needed, you must use getVideoCamera
         * to create the instance if not done yet and obtain a shared pointer
         * for your own usage.
         * VideoManager instance doesn't increment the reference count of
         * this video input instance: this instance is destroyed when the last
         * external user has released its shared pointer.
         */
        std::weak_ptr<sfl_video::VideoInput> videoInput_ = {};
        std::shared_ptr<sfl_video::VideoFrameActiveWriter> videoPreview_ = nullptr;
        sfl_video::VideoDeviceMonitor videoDeviceMonitor_ = {};

    public:
#if HAVE_DBUS
        VideoManager(DBus::Connection& connection);
#else
        VideoManager();
#endif
        sfl_video::VideoDeviceMonitor& getVideoDeviceMonitor();

        std::vector<std::map<std::string, std::string> >
        getCodecs(const std::string& accountID);

        void
        setCodecs(const std::string& accountID,
                  const std::vector<std::map<std::string, std::string> > &details);

        std::vector<std::string>
        getDeviceList();

        sfl_video::VideoCapabilities
        getCapabilities(const std::string& name);

        std::map<std::string, std::string>
        getSettings(const std::string& name);

        void
        applySettings(const std::string& name, const std::map<std::string, std::string>& settings);

        void
        setDefaultDevice(const std::string& name);

        std::string
        getDefaultDevice();

        std::string
        getCurrentCodecName(const std::string &callID);

        std::atomic_bool started_ = {false};
        void startCamera();
        void stopCamera();
        bool hasCameraStarted();
        bool switchInput(const std::string& resource);
        bool switchToCamera();
        std::shared_ptr<sfl_video::VideoFrameActiveWriter> getVideoCamera();

        /* the following signals must be implemented manually for any
         * platform or configuration that does not supply dbus */
#if !HAVE_DBUS
        void deviceEvent();
        void startedDecoding(const std::string &id, const std::string, int w, int h);
        void stoppedDecoding(const std::string &id, const std::string);
#endif // !HAVE_DBUS
};

#endif // VIDEOMANAGER_H_
