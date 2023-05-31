/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#ifndef __RING_DBUSVIDEOMANAGER_H__
#define __RING_DBUSVIDEOMANAGER_H__

#include <vector>
#include <map>
#include <string>

#include "def.h"
#include "dbus_cpp.h"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbusvideomanager.adaptor.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

class LIBJAMI_PUBLIC DBusVideoManager :
    public cx::ring::Ring::VideoManager_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor
{
    public:
        DBusVideoManager(DBus::Connection& connection);

        // Methods
        std::vector<std::string> getDeviceList();
        std::map<std::string, std::map<std::string, std::vector<std::string>>> getCapabilities(const std::string& deviceId);
        std::map<std::string, std::string> getSettings(const std::string& deviceId);
        void applySettings(const std::string& deviceId, const std::map<std::string, std::string>& settings);
        void setDefaultDevice(const std::string& deviceId);
        std::string getDefaultDevice();
        void startAudioDevice();
        void stopAudioDevice();
        bool getDecodingAccelerated();
        void setDecodingAccelerated(const bool& state);
        bool getEncodingAccelerated();
        void setEncodingAccelerated(const bool& state);
        void setDeviceOrientation(const std::string& deviceId, const int& angle);
        void startShmSink(const std::string& sinkId, const bool& value);
        std::map<std::string, std::string> getRenderer(const std::string& callId);
        std::string startLocalMediaRecorder(const std::string& videoInputId, const std::string& filepath);
        void stopLocalRecorder(const std::string& filepath);
        std::string openVideoInput(const std::string& inputUri);
        bool closeVideoInput(const std::string& inputId);
};

#endif // __RING_DBUSVIDEOMANAGER_H__
