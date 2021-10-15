/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "jami/videomanager_interface.h"

DBusVideoManager::DBusVideoManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/VideoManager")
{}

auto
DBusVideoManager::getDeviceList() -> decltype(DRing::getDeviceList())
{
    return DRing::getDeviceList();
}

auto
DBusVideoManager::getCapabilities(const std::string& deviceId) -> decltype(DRing::getCapabilities(deviceId))
{
    return DRing::getCapabilities(deviceId);
}

auto
DBusVideoManager::getSettings(const std::string& deviceId) -> decltype(DRing::getSettings(deviceId))
{
    return DRing::getSettings(deviceId);
}

void
DBusVideoManager::applySettings(const std::string& deviceId, const std::map<std::string, std::string>& settings)
{
    DRing::applySettings(deviceId, settings);
}

void
DBusVideoManager::setDefaultDevice(const std::string& deviceId)
{
    DRing::setDefaultDevice(deviceId);
}

auto
DBusVideoManager::getDefaultDevice() -> decltype(DRing::getDefaultDevice())
{
    return DRing::getDefaultDevice();
}

void
DBusVideoManager::startAudioDevice()
{
    DRing::startAudioDevice();
}

void
DBusVideoManager::stopAudioDevice()
{
    DRing::stopAudioDevice();
}

std::string
DBusVideoManager::openVideoInput(const std::string& inputUri)  {
    return DRing::openVideoInput(inputUri);
}

bool
DBusVideoManager::closeVideoInput(const std::string& inputId) {
    return DRing::closeVideoInput(inputId);
}

auto
DBusVideoManager::getDecodingAccelerated() -> decltype(DRing::getDecodingAccelerated())
{
    return DRing::getDecodingAccelerated();
}

void
DBusVideoManager::setDecodingAccelerated(const bool& state)
{
    DRing::setDecodingAccelerated(state);
}

auto
DBusVideoManager::getEncodingAccelerated() -> decltype(DRing::getEncodingAccelerated())
{
    return DRing::getEncodingAccelerated();
}

void
DBusVideoManager::setEncodingAccelerated(const bool& state)
{
    DRing::setEncodingAccelerated(state);
}

void
DBusVideoManager::setDeviceOrientation(const std::string& deviceId, const int& angle)
{
    DRing::setDeviceOrientation(deviceId, angle);
}

std::map<std::string, std::string>
DBusVideoManager::getRenderer(const std::string& callId)
{
    return DRing::getRenderer(callId);
}

std::string
DBusVideoManager::startLocalMediaRecorder(const std::string& videoInputId, const std::string& filepath)
{
    return DRing::startLocalMediaRecorder(videoInputId, filepath);
}

void
DBusVideoManager::stopLocalRecorder(const std::string& filepath)
{
    DRing::stopLocalRecorder(filepath);
}
