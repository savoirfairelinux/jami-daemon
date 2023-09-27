/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
 *  Author: Vladimir Stoiakin <vstoiakin@lavabit.com>
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

#pragma once

#include "dbusvideomanager.adaptor.h"
#include <videomanager_interface.h>

class DBusVideoManager : public sdbus::AdaptorInterfaces<cx::ring::Ring::VideoManager_adaptor>
{
public:
    DBusVideoManager(sdbus::IConnection& connection)
        : AdaptorInterfaces(connection, "/cx/ring/Ring/VideoManager")
    {
        registerAdaptor();
        registerSignalHandlers();
    }

    ~DBusVideoManager()
    {
        unregisterAdaptor();
    }

    auto
    getDeviceList() -> decltype(libjami::getDeviceList())
    {
        return libjami::getDeviceList();
    }

    auto
    getCapabilities(const std::string& deviceId) -> decltype(libjami::getCapabilities(deviceId))
    {
        return libjami::getCapabilities(deviceId);
    }

    auto
    getSettings(const std::string& deviceId) -> decltype(libjami::getSettings(deviceId))
    {
        return libjami::getSettings(deviceId);
    }

    void
    applySettings(const std::string& deviceId, const std::map<std::string, std::string>& settings)
    {
        libjami::applySettings(deviceId, settings);
    }

    void
    setDefaultDevice(const std::string& deviceId)
    {
        libjami::setDefaultDevice(deviceId);
    }

    auto
    getDefaultDevice() -> decltype(libjami::getDefaultDevice())
    {
        return libjami::getDefaultDevice();
    }

    void
    startAudioDevice()
    {
        libjami::startAudioDevice();
    }

    void
    stopAudioDevice()
    {
        libjami::stopAudioDevice();
    }

    std::string
    openVideoInput(const std::string& inputUri)  {
        return libjami::openVideoInput(inputUri);
    }

    bool
    closeVideoInput(const std::string& inputId) {
        return libjami::closeVideoInput(inputId);
    }

    auto
    getDecodingAccelerated() -> decltype(libjami::getDecodingAccelerated())
    {
        return libjami::getDecodingAccelerated();
    }

    void
    setDecodingAccelerated(const bool& state)
    {
        libjami::setDecodingAccelerated(state);
    }

    auto
    getEncodingAccelerated() -> decltype(libjami::getEncodingAccelerated())
    {
        return libjami::getEncodingAccelerated();
    }

    void
    setEncodingAccelerated(const bool& state)
    {
        libjami::setEncodingAccelerated(state);
    }

    void
    setDeviceOrientation(const std::string& deviceId, const int& angle)
    {
        libjami::setDeviceOrientation(deviceId, angle);
    }

    void
    startShmSink(const std::string& sinkId, const bool& value)
    {
        libjami::startShmSink(sinkId, value);
    }

    std::map<std::string, std::string>
    getRenderer(const std::string& callId)
    {
        return libjami::getRenderer(callId);
    }

    std::string
    startLocalMediaRecorder(const std::string& videoInputId, const std::string& filepath)
    {
        return libjami::startLocalMediaRecorder(videoInputId, filepath);
    }

    void
    stopLocalRecorder(const std::string& filepath)
    {
        libjami::stopLocalRecorder(filepath);
    }

    std::string
    createMediaPlayer(const std::string& path)
    {
        return libjami::createMediaPlayer(path);
    }

    bool
    pausePlayer(const std::string& id, const bool& pause)
    {
        return libjami::pausePlayer(id, pause);
    }

    bool
    closeMediaPlayer(const std::string& id)
    {
        return libjami::closeMediaPlayer(id);
    }

    bool
    mutePlayerAudio(const std::string& id, const bool& mute)
    {
        return libjami::mutePlayerAudio(id, mute);
    }

    bool
    playerSeekToTime(const std::string& id, const int& time)
    {
        return libjami::playerSeekToTime(id, time);
    }

    int64_t
    getPlayerPosition(const std::string& id)
    {
        return libjami::getPlayerPosition(id);
    }

    int64_t
    getPlayerDuration(const std::string& id)
    {
        return libjami::getPlayerDuration(id);
    }

    void
    setAutoRestart(const std::string& id, const bool& restart)
    {
        libjami::setAutoRestart(id, restart);
    }

private:

    void
    registerSignalHandlers()
    {
        using namespace std::placeholders;

        using libjami::exportable_serialized_callback;
        using libjami::VideoSignal;
        using libjami::MediaPlayerSignal;
        using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;

        const std::map<std::string, SharedCallback> videoEvHandlers = {
            exportable_serialized_callback<VideoSignal::DeviceEvent>(
                std::bind(&DBusVideoManager::emitDeviceEvent, this)),
            exportable_serialized_callback<VideoSignal::DecodingStarted>(
                std::bind(&DBusVideoManager::emitDecodingStarted, this, _1, _2, _3, _4, _5)),
            exportable_serialized_callback<VideoSignal::DecodingStopped>(
                std::bind(&DBusVideoManager::emitDecodingStopped, this, _1, _2, _3)),
            exportable_serialized_callback<MediaPlayerSignal::FileOpened>(
                std::bind(&DBusVideoManager::emitFileOpened, this, _1, _2)),
        };

        libjami::registerSignalHandlers(videoEvHandlers);
    }
};
