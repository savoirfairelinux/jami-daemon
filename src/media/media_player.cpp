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
    if (access(path_.c_str(), R_OK) != 0) {
        id_ = "";
        JAMI_ERR() << "File '" << path_ << "' not available";
        return;
    }
    path_ = path;
    id_ = std::to_string(rand());
    audioInput_ = jami::getAudioInput(path_);
    videoInput_ = jami::getVideoInput(path_,
                                      video::VideoInputMode::ManagedByDaemon);

    demuxer_ = std::make_shared<MediaDemuxer>();
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
    DeviceParams devOpts = {};
    devOpts.input = path_;
    devOpts.name = path_;
    devOpts.loop = "1";
    demuxer_->openInput(devOpts);
    demuxer_->findStreamInfo();

    pauseInterval_ = 0;
    startTime_ = av_gettime();
    lastPausedTime_ = startTime_;

    try {
        audioStream_ = demuxer_->selectStream(AVMEDIA_TYPE_AUDIO);
        audioInput_->configureFilePlayback(path_, demuxer_, audioStream_);
        audioInput_->updateStartTime(startTime_);
    } catch (const std::exception& e) {}
    try {
        videoStream_ = demuxer_->selectStream(AVMEDIA_TYPE_VIDEO);
        videoInput_->setSink(id_);
        videoInput_->configureFilePlayback(path_, demuxer_, videoStream_);
        videoInput_->updateStartTime(startTime_);
    } catch (const std::exception& e) {}

    fileDuration_ = demuxer_->getDuration();

    loop_.start();

    std::map<std::string, std::string> info;
    info.insert(std::pair<std::string, std::string>("duration",
    std::to_string(fileDuration_)));

    emitSignal<DRing::MediaPlayerSignal::FileOpened>(id_, info);
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
    if (hasAudio()) {
        audioInput_->updateStartTime(startTime_);
    }
    if (hasVideo()) {
        videoInput_->updateStartTime(startTime_);
    }
}

void
MediaPlayer::muteAudio(bool mute)
{
    if (hasAudio()) {
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
    if (hasAudio()) {
        audioInput_->updateStartTime(newTime);
    }
    if (hasVideo()) {
        videoInput_->updateStartTime(newTime);
    }
}

bool
MediaPlayer::seekToTime(int64_t time) {
    if (time < 0 || time > fileDuration_) {
        return false;
    }
    if (demuxer_->seekFrame(-1, 0)) {
        startTime_ = av_gettime() - pauseInterval_ - time;
        if (hasAudio()) {
            audioInput_->updateStartTime(startTime_);
        }
        if (hasVideo()) {
            videoInput_->updateStartTime(startTime_);
        }
        return true;
    }
    return false;
}

const std::string&
MediaPlayer::getId()
{
    return id_;
}
}// namespace jami

