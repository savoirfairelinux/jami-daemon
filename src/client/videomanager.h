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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory> // for weak/shared_ptr
#include <vector>
#include <map>
#include <mutex>
#include <string>

#include "media/audio/audio_input.h"
#ifdef ENABLE_VIDEO
#include "media/video/video_device_monitor.h"
#include "media/video/video_base.h"
#include "media/video/video_input.h"
#endif
#include "media/media_player.h"

namespace jami {

struct VideoManager
{
public:
    // Client-managed video inputs and players
    std::map<std::string, std::shared_ptr<MediaPlayer>> mediaPlayers;
    // Client-managed audio preview
    std::shared_ptr<AudioInput> audioPreview;

#ifdef ENABLE_VIDEO
    std::map<std::string, std::shared_ptr<video::VideoInput>> clientVideoInputs;
    void setDeviceOrientation(const std::string& deviceId, int angle);
    // device monitor
    video::VideoDeviceMonitor videoDeviceMonitor;
    std::shared_ptr<video::VideoInput> getVideoInput(std::string_view id) const
    {
        auto input = videoInputs.find(id);
        return input == videoInputs.end() ? nullptr : input->second.lock();
    }
    std::mutex videoMutex;
    std::map<std::string, std::weak_ptr<video::VideoInput>, std::less<>> videoInputs;
#endif

    /**
     * Cache of the active Audio/Video input(s).
     */
    std::map<std::string, std::weak_ptr<AudioInput>, std::less<>> audioInputs;
    std::mutex audioMutex;
    bool hasRunningPlayers();
};

#ifdef ENABLE_VIDEO
video::VideoDeviceMonitor& getVideoDeviceMonitor();
std::shared_ptr<video::VideoInput> getVideoInput(
    const std::string& id,
    video::VideoInputMode inputMode = video::VideoInputMode::Undefined,
    const std::string& sink = "");
#endif
std::shared_ptr<AudioInput> getAudioInput(const std::string& id);
std::string createMediaPlayer(const std::string& path);
std::shared_ptr<MediaPlayer> getMediaPlayer(const std::string& id);
bool pausePlayer(const std::string& id, bool pause);
bool closeMediaPlayer(const std::string& id);
bool mutePlayerAudio(const std::string& id, bool mute);
bool playerSeekToTime(const std::string& id, int time);
int64_t getPlayerPosition(const std::string& id);
int64_t getPlayerDuration(const std::string& id);

} // namespace jami
