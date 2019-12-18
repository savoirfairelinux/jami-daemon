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
#include "client/ring_signal.h"
#include <string>
namespace jami {

MediaPlayer::MediaPlayer(const std::string& path)
{
    path_ = path;
    id_ = std::to_string(rand());
    thread_ = std::thread([this] {
        initMediaInputs();
    });
}

MediaPlayer::~MediaPlayer()
{
    thread_.join();
}

void
MediaPlayer::initMediaInputs()
{
    audioInput_ = jami::getAudioInput(path_);
    videoInput_ = jami::getVideoInput(path_,
                                      video::VideoInputMode::ManagedByDaemon);
    videoInput_->setPaused(true);
    audioInput_->setPaused(true);
    audioInput_->switchInput(path_);
    videoInput_->setSink(id_);
    auto paramsFuture = videoInput_->switchInput(path_, false);
    try {
        if (paramsFuture.valid()
            && paramsFuture.wait_for(NEWPARAMS_TIMEOUT) == std::future_status::ready) {
            auto params = paramsFuture.get();
            if (params.width == 0 || params.height == 0) {
                videoInput_ = nullptr;
                hasVideo = false;
            }
        } else {
            videoInput_ = nullptr;
            hasVideo = false;
        }
    } catch (const std::exception& e) {
        videoInput_ = nullptr;
        hasVideo = false;
    }
    pauseInterval_ = 0;
    startTime_ = av_gettime();
    lastPausedTime_ = startTime_;
    audioInput_->setFileFinishedCallback([this]() -> void {
        audioFinished_ = true;
        if (videoFinished_ || !hasVideo) {
            playFileFromBeginning();
        }
    });
    audioInput_->updateStartTime(startTime_);
    if (!hasVideo) {
        return;
    }
    videoInput_->updateStartTime(startTime_);

    videoInput_->setFileFinishedCallback([this]() -> void {
        videoFinished_ = true;
        if (audioFinished_) {
            playFileFromBeginning();
        }
    });

    double duration = (double)audioInput_->duration() / AV_TIME_BASE;
    emitSignal<DRing::MediaPlayerSignal::FileOpened>(id_, duration);
}

void
MediaPlayer::playFileFromBeginning()
{
    audioFinished_ = false;
    startTime_ = av_gettime();
    lastPausedTime_ = startTime_;
    pauseInterval_ = 0;
    if (audioInput_) {
        audioInput_->createDecoder();
        audioInput_->updateStartTime(startTime_);
        audioInput_->setPaused(false);
    }
    if (videoInput_) {
        videoFinished_ = false;
        videoInput_->createDecoder();
        videoInput_->updateStartTime(startTime_);
        videoInput_->setPaused(false);
    }
}

void
MediaPlayer::muteAudio(bool mute)
{
    if (audioInput_) {
        audioInput_->setMuted(mute);
    }
}

void
MediaPlayer::pause(bool pause)
{
    if (!pause) {
        pauseInterval_ += av_gettime() - lastPausedTime_;
    } else {
        lastPausedTime_ = av_gettime();
    }
    auto newTime = startTime_ + pauseInterval_;
    if (audioInput_) {
        audioInput_->updateStartTime(newTime);
        audioInput_->setPaused(pause);
    }
    if (videoInput_) {
        videoInput_->updateStartTime(newTime);
        videoInput_->setPaused(pause);
    }
}

const std::string&
MediaPlayer::getId()
{
    return id_;
}
}// namespace jami

