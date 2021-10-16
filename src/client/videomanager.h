/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#include "audio/audio_input.h"
#include "video/video_device_monitor.h"
#include "video/video_base.h"
#include "video/video_input.h"
#include "media_player.h"

namespace jami {

struct VideoManager
{
public:
    void setDeviceOrientation(const std::string& deviceId, int angle);

    // Client-managed video inputs and players
    std::map<std::string, std::shared_ptr<video::VideoInput>> clientVideoInputs;
    std::map<std::string, std::shared_ptr<MediaPlayer>> mediaPlayers;
    // Client-managed audio preview
    std::shared_ptr<AudioInput> audioPreview;

    // device monitor
    video::VideoDeviceMonitor videoDeviceMonitor;

    /**
     * Cache of the active Audio/Video input(s).
     */
    std::map<std::string, std::weak_ptr<AudioInput>, std::less<>> audioInputs;
    std::map<std::string, std::weak_ptr<video::VideoInput>, std::less<>> videoInputs;
    std::mutex audioMutex;
    bool hasRunningPlayers();
    std::shared_ptr<video::VideoInput> getVideoInput(std::string_view id) const {
        auto input = videoInputs.find(id);
        return input == videoInputs.end() ? nullptr : input->second.lock();
    }
};

video::VideoDeviceMonitor& getVideoDeviceMonitor();
std::shared_ptr<AudioInput> getAudioInput(const std::string& id);
std::shared_ptr<video::VideoInput> getVideoInput(const std::string& id, video::VideoInputMode inputMode = video::VideoInputMode::Undefined);
std::string createMediaPlayer(const std::string& path);
std::shared_ptr<MediaPlayer> getMediaPlayer(const std::string& id);
bool pausePlayer(const std::string& id, bool pause);
bool closePlayer(const std::string& id);
bool mutePlayerAudio(const std::string& id, bool mute);
bool playerSeekToTime(const std::string& id, int time);
int64_t getPlayerPosition(const std::string& id);

} // namespace jami
