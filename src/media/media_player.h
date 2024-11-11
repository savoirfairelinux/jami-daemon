/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "audio/audio_input.h"
#ifdef ENABLE_VIDEO
#include "video/video_input.h"
#endif
#include "media_decoder.h"
#include <atomic>

namespace jami {
class MediaPlayer
{
public:
    MediaPlayer(const std::string& resource);
    ~MediaPlayer();

    void pause(bool pause);
    bool isInputValid();
    const std::string& getId() const;
    void muteAudio(bool mute);
    bool seekToTime(int64_t time);
    int64_t getPlayerPosition() const;
    int64_t getPlayerDuration() const;
    bool isPaused() const;
    void setAutoRestart(bool state) { autoRestart_ = state; }

private:
    std::string path_;
    bool autoRestart_ {false};

    // media inputs
#ifdef ENABLE_VIDEO
    std::shared_ptr<jami::video::VideoInput> videoInput_;
#endif
    std::shared_ptr<jami::AudioInput> audioInput_;
    std::shared_ptr<MediaDemuxer> demuxer_;
    ThreadLoop loop_;

    int64_t startTime_;
    int64_t lastPausedTime_;
    int64_t pauseInterval_;

    inline bool hasAudio() const { return audioStream_ >= 0; }

    inline bool hasVideo() const { return videoStream_ >= 0; }

    int audioStream_ = -1;
    int videoStream_ = -1;
    int64_t fileDuration_ = 0;

    void playFileFromBeginning();
    std::atomic_bool paused_ {true};
    bool readBufferOverflow_ = false;
    bool audioStreamEnded_ {false};
    bool videoStreamEnded_ {false};

    bool configureMediaInputs();
    void process();

    void emitInfo();
    void flushMediaBuffers();

    bool streamsFinished();
};
} // namespace jami
