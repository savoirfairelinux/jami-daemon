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

static constexpr auto MS_PER_PACKET = std::chrono::milliseconds(20);

MediaPlayer::MediaPlayer(const std::string& path)
:loop_([] { return true; },
        [this] { process(); },
        [] {})
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
    demuxer_ = std::make_shared<MediaDemuxer>();
    DeviceParams devOpts = {};
    devOpts.input = path_;
    devOpts.name = path_;
    devOpts.loop = "1";
    demuxer_->openInput(devOpts);
    demuxer_->findStreamInfo();
    bool noVideo = false;
    bool noAudio = false;
    try {
        int audioStream = demuxer_->selectStream(AVMEDIA_TYPE_AUDIO);
        audioInput_->playFile(path_, demuxer_, audioStream);
    } catch (const std::exception& e) {
        noAudio = true;
    }
    try {
    int videoStream = demuxer_->selectStream(AVMEDIA_TYPE_VIDEO);
        videoInput_->setSink(id_);
        videoInput_->playFile(path_, demuxer_, videoStream);
    } catch (const std::exception& e) {
        noVideo = true;
    }
    pauseInterval_ = 0;
    startTime_ = av_gettime();
    lastPausedTime_ = startTime_;
    audioInput_->updateStartTime(startTime_);
    videoInput_->updateStartTime(startTime_);
    loop_.start();

    double duration = (double)demuxer_->getDuration() / AV_TIME_BASE;
    //emitSignal<DRing::MediaPlayerSignal::FileOpened>(id_, duration);
}

void
MediaPlayer::process()
{
    if (!demuxer_)
        return;
    if (paused_) {
        std::this_thread::sleep_for(MS_PER_PACKET);
        return;
    }
    demuxer_->decode();
    const auto ret = demuxer_->decode();
    switch (ret) {
    case MediaDemuxer::Status::Success:
        break;
    case MediaDemuxer::Status::EndOfFile:
            playFileFromBeginning();
        break;
    case MediaDemuxer::Status::ReadError:
        JAMI_ERR() << "Failed to decode frame";
        break;
    }
}


void
MediaPlayer::playFileFromBeginning()
{
    if (!demuxer_->seekFrame(-1, 0)) {
        return;
    }
    startTime_ = av_gettime();
    lastPausedTime_ = startTime_;
    pauseInterval_ = 0;
    if (audioInput_) {
        audioInput_->updateStartTime(startTime_);
    }
    if (videoInput_) {
        videoInput_->updateStartTime(startTime_);
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
    paused_ = pause;
    auto newTime = startTime_ + pauseInterval_;
    if (audioInput_) {
          audioInput_->updateStartTime(newTime);
    }
    if (videoInput_) {
        videoInput_->updateStartTime(newTime);
    }
}

const std::string&
MediaPlayer::getId()
{
    return id_;
}
}// namespace jami

