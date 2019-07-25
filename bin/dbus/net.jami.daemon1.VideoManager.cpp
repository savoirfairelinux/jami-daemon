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

auto
getDeviceList() -> decltype(DRing::getDeviceList())
{
    return DRing::getDeviceList();
}

auto
getCapabilities(const std::string& name) -> decltype(DRing::getCapabilities(name))
{
    return DRing::getCapabilities(name);
}

auto
getSettings(const std::string& name) -> decltype(DRing::getSettings(name))
{
    return DRing::getSettings(name);
}

void
applySettings(const std::string& name, const std::map<std::string, std::string>& settings)
{
    DRing::applySettings(name, settings);
}

void
setDefaultDevice(const std::string& dev)
{
    DRing::setDefaultDevice(dev);
}

auto
getDefaultDevice() -> decltype(DRing::getDefaultDevice())
{
    return DRing::getDefaultDevice();
}

void
startCamera()
{
    DRing::startCamera();
}

void
stopCamera()
{
    DRing::stopCamera();
}

void
startAudioDevice()
{
    DRing::startAudioDevice();
}

void
stopAudioDevice()
{
    DRing::stopAudioDevice();
}

auto
switchInput(const std::string& resource) -> decltype(DRing::switchInput(resource))
{
    return DRing::switchInput(resource);
}

auto
hasCameraStarted() -> decltype(DRing::hasCameraStarted())
{
    return DRing::hasCameraStarted();
}

auto
getDecodingAccelerated() -> decltype(DRing::getDecodingAccelerated())
{
    return DRing::getDecodingAccelerated();
}

void
setDecodingAccelerated(const bool& state)
{
    DRing::setDecodingAccelerated(state);
}

auto
getEncodingAccelerated() -> decltype(DRing::getEncodingAccelerated())
{
    return DRing::getEncodingAccelerated();
}

void
setEncodingAccelerated(const bool& state)
{
    DRing::setEncodingAccelerated(state);
}

void
setDeviceOrientation(const std::string& name, const int& angle)
{
    DRing::setDeviceOrientation(name, angle);
}

std::map<std::string, std::string>
getRenderer(const std::string& callId)
{
    return DRing::getRenderer(callId);
}

std::string
startLocalRecorder(const bool& audioOnly, const std::string& filepath)
{
    return DRing::startLocalRecorder(audioOnly, filepath);
}

void
stopLocalRecorder(const std::string& filepath)
{
    DRing::stopLocalRecorder(filepath);
}
