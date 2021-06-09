/*
 *  Copyright (C)2020-2021 Savoir-faire Linux Inc.
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
#include "jamid/media_const.h"
#include "manager.h"
#include <string>
namespace jami {

static constexpr auto MS_PER_PACKET = std::chrono::milliseconds(20);

MediaPlayer::MediaPlayer(const std::string& path)
    : loop_(std::bind(&MediaPlayer::configureMediaInputs, this),
            std::bind(&MediaPlayer::process, this),
            [] {})
{
    static const std::string& sep = DRing::Media::VideoProtocolPrefix::SEPARATOR;
    const auto pos = path.find(sep);
    const auto suffix = path.substr(pos + sep.size());

    if (access(suffix.c_str(), R_OK) != 0) {
        JAMI_ERR() << "File '" << path << "' not available";
        return;
    }

    path_ = path;
    id_ = std::to_string(rand());
    audioInput_ = jami::getAudioInput(id_);
    audioInput_->setPaused(paused_);
    videoInput_ = jami::getVideoInput(id_, video::VideoInputMode::ManagedByDaemon);
    videoInput_->setPaused(paused_);

    demuxer_ = std::make_shared<MediaDemuxer>();
    loop_.start();
}

MediaPlayer::~MediaPlayer()
{
    loop_.join();
}

bool
MediaPlayer::configureMediaInputs()
{
    DeviceParams devOpts = {};
    devOpts.input = path_;
    devOpts.name = path_;
    devOpts.loop = "1";

    if (demuxer_->openInput(devOpts) < 0) {
        emitInfo();
        return false;
    }
    demuxer_->findStreamInfo();

    pauseInterval_ = 0;
    startTime_ = av_gettime();
    lastPausedTime_ = startTime_;

    try {
        audioStream_ = demuxer_->selectStream(AVMEDIA_TYPE_AUDIO);
        if (hasAudio()) {
            audioInput_->configureFilePlayback(path_, demuxer_, audioStream_);
            audioInput_->updateStartTime(startTime_);
            audioInput_->start();
        }
    } catch (const std::exception& e) {
        JAMI_ERR("media player: %s open audio input failed: %s", path_.c_str(), e.what());
    }
    try {
        videoStream_ = demuxer_->selectStream(AVMEDIA_TYPE_VIDEO);
        if (hasVideo()) {
            videoInput_->setSink(id_);
            videoInput_->configureFilePlayback(path_, demuxer_, videoStream_);
            videoInput_->updateStartTime(startTime_);
            muteAudio(true);
        }
    } catch (const std::exception& e) {
        videoInput_ = nullptr;
        JAMI_ERR("media player: %s open video input failed: %s", path_.c_str(), e.what());
    }

    demuxer_->setNeedFrameCb([this]() -> void { readBufferOverflow_ = false; });

    demuxer_->setFileFinishedCb([this](bool isAudio) -> void {
        if (isAudio) {
            audioStreamEnded_ = true;
        } else {
            videoStreamEnded_ = true;
        }
    });

    fileDuration_ = demuxer_->getDuration();
    if (fileDuration_ <= 0) {
        emitInfo();
        return false;
    }
    emitInfo();
    demuxer_->updateCurrentState(MediaDemuxer::CurrentState::Demuxing);
    return true;
}

void
MediaPlayer::process()
{
    if (!demuxer_)
        return;
    if (streamsFinished()) {
        audioStreamEnded_ = false;
        videoStreamEnded_ = false;
        playFileFromBeginning();
    }

    if (paused_ || readBufferOverflow_) {
        std::this_thread::sleep_for(MS_PER_PACKET);
        return;
    }

    const auto ret = demuxer_->demuxe();
    switch (ret) {
    case MediaDemuxer::Status::Success:
    case MediaDemuxer::Status::FallBack:
        break;
    case MediaDemuxer::Status::EndOfFile:
        demuxer_->updateCurrentState(MediaDemuxer::CurrentState::Finished);
        break;
    case MediaDemuxer::Status::ReadError:
        JAMI_ERR() << "Failed to decode frame";
        break;
    case MediaDemuxer::Status::ReadBufferOverflow:
        readBufferOverflow_ = true;
        break;
    }
}

void
MediaPlayer::emitInfo()
{
    std::map<std::string, std::string> info {{"duration", std::to_string(fileDuration_)},
                                             {"audio_stream", std::to_string(audioStream_)},
                                             {"video_stream", std::to_string(videoStream_)}};
    emitSignal<DRing::MediaPlayerSignal::FileOpened>(id_, info);
}

bool
MediaPlayer::isInputValid()
{
    return !id_.empty();
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
    paused_ = pause;
    if (!pause) {
        pauseInterval_ += av_gettime() - lastPausedTime_;
    } else {
        lastPausedTime_ = av_gettime();
    }
    auto newTime = startTime_ + pauseInterval_;
    if (hasAudio()) {
        audioInput_->setPaused(paused_);
        audioInput_->updateStartTime(newTime);
    }
    if (hasVideo()) {
        videoInput_->setPaused(paused_);
        videoInput_->updateStartTime(newTime);
    }
}

bool
MediaPlayer::seekToTime(int64_t time)
{
    if (time < 0 || time > fileDuration_) {
        return false;
    }
    if (time >= fileDuration_) {
        playFileFromBeginning();
        return true;
    }
    if (!demuxer_->seekFrame(-1, time)) {
        return false;
    }
    flushMediaBuffers();
    demuxer_->updateCurrentState(MediaDemuxer::CurrentState::Demuxing);
    startTime_ = av_gettime() - pauseInterval_ - time;
    if (hasAudio()) {
        audioInput_->setSeekTime(time);
        audioInput_->updateStartTime(startTime_);
    }
    if (hasVideo()) {
        videoInput_->setSeekTime(time);
        videoInput_->updateStartTime(startTime_);
    }
    return true;
}
void
MediaPlayer::playFileFromBeginning()
{
    pause(true);
    demuxer_->updateCurrentState(MediaDemuxer::CurrentState::Demuxing);
    if (!demuxer_->seekFrame(-1, 0)) {
        return;
    }
    flushMediaBuffers();
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
MediaPlayer::flushMediaBuffers()
{
    if (hasVideo()) {
        videoInput_->flushBuffers();
    }

    if (hasAudio()) {
        audioInput_->flushBuffers();
    }
}

const std::string&
MediaPlayer::getId() const
{
    return id_;
}

int64_t
MediaPlayer::getPlayerPosition() const
{
    if (paused_) {
        return lastPausedTime_ - startTime_ - pauseInterval_;
    }
    return av_gettime() - startTime_ - pauseInterval_;
}

bool
MediaPlayer::isPaused() const
{
    return paused_;
}

bool
MediaPlayer::streamsFinished()
{
    bool audioFinished = hasAudio() ? audioStreamEnded_ : true;
    bool videoFinished = hasVideo() ? videoStreamEnded_ : true;
    return audioFinished && videoFinished;
}

} // namespace jami
