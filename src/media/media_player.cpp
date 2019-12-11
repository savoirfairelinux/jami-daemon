/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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
    sinkId_ = std::to_string(rand());
    audioInput_ = jami::getAudioInput(path);
    videoInput_ = std::static_pointer_cast<video::VideoInput>(jami::getVideoInput(path));
    videoInput_->setSink(sinkId_);
    videoInput_->setPaused(true);
    audioInput_->setPaused(true);
    audioInput_->switchInput(path);
    pausedInterval_ = 0;
    startTime_ = av_gettime();
    lastPausedTime_ = startTime_;
    auto newParams = videoInput_->switchInput(path, false).get();
    if (newParams.width == 0 || newParams.height == 0){
        videoInput_ = nullptr;
    }
    videoInput_->updateStartTime(startTime_);
    audioInput_->updateStartTime(startTime_);
    //audioInput_->switchInput(path);
//    if(videoInput_) {
//        audioInput_->emulateRate();
//    }
  // startTime_ = AV_NOPTS_VALUE;
  //  lastPausedTime_ = AV_NOPTS_VALUE;
    if (audioInput_) {
           audioStream_ = audioInput_->getStream(path);
    }

    if (videoInput_) {
          videoStream_ = videoInput_->getStream(path);
    }

    audioInput_->setFileFinishedCallback(
    [this]() -> int {
        audioFinished_ = true;
        if (videoFinished_) {
            startFile();
        }
        return 1;
    });
    videoInput_->setFileFinishedCallback(
       [this]() -> int {
           videoFinished_ = true;
                  if (audioFinished_) {
                      startFile();
                  }
           return 1;
       });

}

void
MediaPlayer::startFile()
{
    audioFinished_ = false;
    videoFinished_ = false;
    startTime_ = av_gettime();
    lastPausedTime_ = av_gettime();
    pausedInterval_ = 0;
    audioInput_->createDecoder();
    videoInput_->createDecoder();
    videoInput_->setPaused(false);
    audioInput_->setPaused(false);
}

MediaPlayer::~MediaPlayer()
{}

void
MediaPlayer::muteAudio(bool mute)
{
    audioInput_->setMuted(mute);
}

bool
MediaPlayer::couldopenInput() {
    return videoInput_ || audioInput_;
}

void
MediaPlayer::pausePlayer(bool pause)
{
  //  auto duration1 = videoInput_->duration();
 //   auto duration2 = audioInput_->duration();
    if (!pause) {
        pausedInterval_ += av_gettime() - lastPausedTime_;
    } else {
        lastPausedTime_ = av_gettime();
    }

//    if (startTime_ != AV_NOPTS_VALUE) {
//          //update time
//        if((av_gettime() - startTime_ - pausedInterval_) > duration2) {
//            int diff = av_gettime() - startTime_;
//            int diffWithPaused = diff - pausedInterval_;
//            int c =  (int) diffWithPaused / duration2;
//           // int c = (int)(av_gettime() - startTime_ - pausedInterval_) / duration2;
//            startTime_ = startTime_ + pausedInterval_ + duration2 * c;
//            pausedInterval_ = 0;
//
//            //file finished and restarted
//
//        }
//    }
    if (audioInput_) {
        audioInput_->updateStartTime(startTime_ + pausedInterval_);
        audioInput_->setPaused(pause);
    }

    if (videoInput_) {
        videoInput_->updateStartTime(startTime_ + pausedInterval_);
        videoInput_->setPaused(pause);
    }
}

void
MediaPlayer::close()
{
    audioInput_->switchInput(NULL);
    videoInput_->switchInput(NULL);
}

const std::string&
MediaPlayer::getSinkId() {
    return sinkId_;
}
}// namespace jami

