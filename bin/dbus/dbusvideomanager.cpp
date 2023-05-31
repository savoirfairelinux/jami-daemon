/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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

#include "dbusvideomanager.h"
#include "videomanager_interface.h"

DBusVideoManager::DBusVideoManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/VideoManager")
{}

auto
DBusVideoManager::getDeviceList() -> decltype(libjami::getDeviceList())
{
    return libjami::getDeviceList();
}

auto
DBusVideoManager::getCapabilities(const std::string& deviceId) -> decltype(libjami::getCapabilities(deviceId))
{
    return libjami::getCapabilities(deviceId);
}

auto
DBusVideoManager::getSettings(const std::string& deviceId) -> decltype(libjami::getSettings(deviceId))
{
    return libjami::getSettings(deviceId);
}

void
DBusVideoManager::applySettings(const std::string& deviceId, const std::map<std::string, std::string>& settings)
{
    libjami::applySettings(deviceId, settings);
}

void
DBusVideoManager::setDefaultDevice(const std::string& deviceId)
{
    libjami::setDefaultDevice(deviceId);
}

auto
DBusVideoManager::getDefaultDevice() -> decltype(libjami::getDefaultDevice())
{
    return libjami::getDefaultDevice();
}

void
DBusVideoManager::startAudioDevice()
{
    libjami::startAudioDevice();
}

void
DBusVideoManager::stopAudioDevice()
{
    libjami::stopAudioDevice();
}

std::string
DBusVideoManager::openVideoInput(const std::string& inputUri)  {
    return libjami::openVideoInput(inputUri);
}

bool
DBusVideoManager::closeVideoInput(const std::string& inputId) {
    return libjami::closeVideoInput(inputId);
}

auto
DBusVideoManager::getDecodingAccelerated() -> decltype(libjami::getDecodingAccelerated())
{
    return libjami::getDecodingAccelerated();
}

void
DBusVideoManager::setDecodingAccelerated(const bool& state)
{
    libjami::setDecodingAccelerated(state);
}

auto
DBusVideoManager::getEncodingAccelerated() -> decltype(libjami::getEncodingAccelerated())
{
    return libjami::getEncodingAccelerated();
}

void
DBusVideoManager::setEncodingAccelerated(const bool& state)
{
    libjami::setEncodingAccelerated(state);
}

void
DBusVideoManager::setDeviceOrientation(const std::string& deviceId, const int& angle)
{
    libjami::setDeviceOrientation(deviceId, angle);
}

void
DBusVideoManager::startShmSink(const std::string& sinkId, const bool& value)
{
    libjami::startShmSink(sinkId, value);
}

std::map<std::string, std::string>
DBusVideoManager::getRenderer(const std::string& callId)
{
    return libjami::getRenderer(callId);
}

std::string
DBusVideoManager::startLocalMediaRecorder(const std::string& videoInputId, const std::string& filepath)
{
    return libjami::startLocalMediaRecorder(videoInputId, filepath);
}

void
DBusVideoManager::stopLocalRecorder(const std::string& filepath)
{
    libjami::stopLocalRecorder(filepath);
}
