/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
#include "client/videomanager.h"

DBusVideoManager::DBusVideoManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/VideoManager")
{}

auto
DBusVideoManager::getDeviceList() -> decltype(DRing::getDeviceList())
{
    return DRing::getDeviceList();
}

auto
DBusVideoManager::getCapabilities(const std::string& name) -> decltype(DRing::getCapabilities(name))
{
    return DRing::getCapabilities(name);
}

auto
DBusVideoManager::getSettings(const std::string& name) -> decltype(DRing::getSettings(name))
{
    return DRing::getSettings(name);
}

void
DBusVideoManager::applySettings(const std::string& name, const std::map<std::string, std::string>& settings)
{
    DRing::applySettings(name, settings);
}

void
DBusVideoManager::setDefaultDevice(const std::string& dev)
{
    DRing::setDefaultDevice(dev);
}

auto
DBusVideoManager::getDefaultDevice() -> decltype(DRing::getDefaultDevice())
{
    return DRing::getDefaultDevice();
}

void
DBusVideoManager::startCamera()
{
    DRing::startCamera();
}

void
DBusVideoManager::stopCamera()
{
    DRing::stopCamera();
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

auto
DBusVideoManager::switchInput(const std::string& resource) -> decltype(DRing::switchInput(resource))
{
    return DRing::switchInput(resource);
}

auto
DBusVideoManager::hasCameraStarted() -> decltype(DRing::hasCameraStarted())
{
    return DRing::hasCameraStarted();
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

void
DBusVideoManager::setDeviceOrientation(const std::string& name, int angle)
{
    DRing::setDeviceOrientation(name, angle);
}

std::map<std::string, std::string>
DBusVideoManager::getRenderer(const std::string& callId)
{
    return DRing::getRenderer(callId);
}

std::string
DBusVideoManager::startLocalRecorder(const bool& audioOnly, const std::string& filepath)
{
    return DRing::startLocalRecorder(audioOnly, filepath);
}

void
DBusVideoManager::stopLocalRecorder(const std::string& filepath)
{
    DRing::stopLocalRecorder(filepath);
}
