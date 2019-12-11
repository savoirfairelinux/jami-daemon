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
    videoInput_->switchInput(path);
    audioInput_->switchInput(path);
   // auto audioStream = audioInput_->getStream(path);
    ///auto videoStream = videoInput_->getStream(path);
//    if (!videoStream.isValid()) {
//        videoInput_ = nullptr;
//    }
//    if (!audioStream.isValid()) {
//        audioInput_ = nullptr;
//    }
}

MediaPlayer::~MediaPlayer()
{}

bool
MediaPlayer::couldopenInput() {
    return videoInput_ || audioInput_;
}

void
MediaPlayer::pausePlayer(bool pause)
{
    if (videoInput_) {
           videoInput_->setPaused(pause);
       }
       if (audioInput_) {
           audioInput_->setPaused(pause);
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

