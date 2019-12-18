/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *
 *  Author: Kateryna Kostiuk <kateryna.kostiuk@savoirfairelinux.com>
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

#include "media_player.h"
#include "client/videomanager.h"
#include <string>
namespace jami {

MediaPlayer::MediaPlayer(const std::string& path)
{
    path_ = path;
    playerId_ = std::to_string(rand());
    std::async([this] {
        initMediaInputs();
    });
}

MediaPlayer::~MediaPlayer()
{}

void
MediaPlayer::initMediaInputs()
{
    audioInput_ = jami::getAudioInput(path_);
    videoInput_ = std::static_pointer_cast<video::VideoInput>(jami::getVideoInput(path_, video::VideoInputMode::ManagedByClient));
    videoInput_->setPaused(true);
    audioInput_->setPaused(true);
    audioInput_->switchInput(path_);
    videoInput_->setSink(playerId_);
    auto params = videoInput_->switchInput(path_, false).get();
    if (params.width == 0 || params.height == 0) {
        videoInput_ = nullptr;
        hasVideo = false;
    }
    pauseInterval_ = 0;
    startTime_ = av_gettime();
    lastPausedTime_ = startTime_;
    audioStream_ = audioInput_->getStream(path_);
    audioInput_->setFileFinishedCallback([this]() -> void {
        audioFinished_ = true;
        if (videoFinished_ || !hasVideo) {
            playFileFromBegining();
        }
    });
    audioInput_->updateStartTime(startTime_);
    if (!hasVideo) {
        return;
    }
    videoInput_->updateStartTime(startTime_);

    videoStream_ = videoInput_->getStream(path_);
    videoInput_->setFileFinishedCallback([this]() -> void {
        videoFinished_ = true;
        if (audioFinished_) {
            playFileFromBegining();
        }
    });

    //emit signal file opened
    auto duration = audioInput_->duration();
}

void
MediaPlayer::playFileFromBegining()
{
    audioFinished_ = false;
    videoFinished_ = false;
    startTime_ = av_gettime();
    lastPausedTime_ = startTime_;
    pauseInterval_ = 0;
    audioInput_->createDecoder();
    audioInput_->updateStartTime(startTime_);
    audioInput_->setPaused(false);
    if (!hasVideo) {
        return;
    }
    videoInput_->createDecoder();
    videoInput_->updateStartTime(startTime_);
    videoInput_->setPaused(false);
}

void
MediaPlayer::muteAudio(bool mute)
{
    audioInput_->setMuted(mute);
}

void
MediaPlayer::pausePlayer(bool pause)
{
    if (!pause) {
        pauseInterval_ += av_gettime() - lastPausedTime_;
    } else {
        lastPausedTime_ = av_gettime();
    }
    auto newTime = startTime_ + pauseInterval_;
    audioInput_->updateStartTime(newTime);
    audioInput_->setPaused(pause);
    if (!hasVideo) {
        return;
    }
    videoInput_->updateStartTime(newTime);
    videoInput_->setPaused(pause);
}

void
MediaPlayer::closePlayer()
{
    audioInput_->switchInput("");
    videoInput_->switchInput("");
}

const std::string&
MediaPlayer::getPlayerId() {
    return playerId_;
}
}// namespace jami

