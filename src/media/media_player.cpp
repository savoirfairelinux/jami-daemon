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
#include "dring/media_const.h"
#include <string>
namespace jami {

static constexpr auto MS_PER_PACKET = std::chrono::milliseconds(20);

MediaPlayer::MediaPlayer(const std::string& path)
:loop_([] { return true; },
        [this] { process(); },
        [] {})
{
    static const std::string sep = DRing::Media::VideoProtocolPrefix::SEPARATOR;
    const auto pos = path.find(sep);
    const auto suffix = path.substr(pos + sep.size());

    if (access(suffix.c_str(), R_OK) != 0) {
        id_ = "";
        JAMI_ERR() << "File '" << path << "' not available";
        return;
    }

    path_ = path;
    id_ = std::to_string(rand());
    audioInput_ = jami::getAudioInput(path_);
    audioInput_->setPaused(true);
    videoInput_ = jami::getVideoInput(path_,
                                      video::VideoInputMode::ManagedByDaemon);

    demuxer_ = std::make_shared<MediaDemuxer>();
    thread_ = std::thread([this] {
        configureMediaInputs();
    });
}

MediaPlayer::~MediaPlayer()
{
    if (thread_.joinable()) {
        thread_.join();
    }
}

void
MediaPlayer::configureMediaInputs()
{
    DeviceParams devOpts = {};
    devOpts.input = path_;
    devOpts.name = path_;
    devOpts.loop = "1";

    if (demuxer_->openInput(devOpts) < 0) {
        return;
    }
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
        muteAudio(true);
    } catch (const std::exception& e) {
        if (hasAudio()) {
            //for files that have only audio audio input manages when do we need a new frame
            audioInput_->setNeedFrameCallback([this]() -> void {
                decode();
            });
        }
    }

    loop_.start();
    fileDuration_ = demuxer_->getDuration();

    std::map<std::string, std::string> info;
    info.insert(std::pair<std::string, std::string>("duration", std::to_string(fileDuration_)));
    info.insert(std::pair<std::string, std::string>("audio_stream", std::to_string(audioStream_)));
    info.insert(std::pair<std::string, std::string>("video_stream", std::to_string(videoStream_)));

    emitSignal<DRing::MediaPlayerSignal::FileOpened>(id_, info);
}

void
MediaPlayer::decode()
{
    if (!demuxer_)
        return;
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
MediaPlayer::process()
{
    if (!hasVideo() && !hasAudio()) {
        loop_.stop();
        return;
    }
    //for audio file only frame read when needed and pause managed by audio input
    if (!hasVideo() && hasAudio()) {
        audioInput_->readFromDevice();
        return;
    }
    if (paused_) {
        std::this_thread::sleep_for(MS_PER_PACKET);
        return;
    }
    decode();
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
        audioInput_->setPaused(paused_);
        audioInput_->updateStartTime(startTime_);
    }
    if (hasVideo()) {
        videoInput_->updateStartTime(startTime_);
        videoInput_->flushBuffers();
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
    if (pause == paused_) {
        return;
    }
    if (!pause) {
        pauseInterval_ += av_gettime() - lastPausedTime_;
    } else {
        lastPausedTime_ = av_gettime();
    }
    paused_ = pause;
    auto newTime = startTime_ + pauseInterval_;
    if (hasAudio()) {
        audioInput_->setPaused(paused_);
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
    if (!demuxer_->seekFrame(-1, time)) {
        return false;
    }
    startTime_ = av_gettime() - pauseInterval_ - time;
    if (hasAudio()) {
        audioInput_->updateStartTime(startTime_);
    }
    if (hasVideo()) {
        videoInput_->updateStartTime(startTime_);
        videoInput_->flushBuffers();
    }
    return true;
}

const std::string&
MediaPlayer::getId()
{
    return id_;
}

int64_t
MediaPlayer::getPlayerPosition() {
    if (paused_ ) {
        return lastPausedTime_ - startTime_ - pauseInterval_;
    }
    return av_gettime() - startTime_ - pauseInterval_;
}
}// namespace jami

