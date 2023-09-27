/*
 *  Copyright (C) 2020-2023 Savoir-faire Linux Inc.
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
#include "jami/media_const.h"
#include "manager.h"
#include <string>
namespace jami {

static constexpr auto MS_PER_PACKET = std::chrono::milliseconds(20);

MediaPlayer::MediaPlayer(const std::string& resource)
    : loop_(std::bind(&MediaPlayer::configureMediaInputs, this),
            std::bind(&MediaPlayer::process, this),
            [] {})
{
    auto suffix = resource;
    static const std::string& sep = libjami::Media::VideoProtocolPrefix::SEPARATOR;
    const auto pos = resource.find(sep);
    if (pos != std::string::npos) {
        suffix = resource.substr(pos + sep.size());
    }

    path_ = suffix;

    audioInput_ = jami::getAudioInput(path_);
    audioInput_->setPaused(paused_);
#ifdef ENABLE_VIDEO
    videoInput_ = jami::getVideoInput(path_, video::VideoInputMode::ManagedByDaemon, resource);
    videoInput_->setPaused(paused_);
#endif

    demuxer_ = std::make_shared<MediaDemuxer>();
    loop_.start();
}

MediaPlayer::~MediaPlayer()
{
    pause(true);
    loop_.join();
    audioInput_.reset();
    videoInput_.reset();
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
#ifdef ENABLE_VIDEO
    try {
        videoStream_ = demuxer_->selectStream(AVMEDIA_TYPE_VIDEO);
        if (hasVideo()) {
            videoInput_->configureFilePlayback(path_, demuxer_, videoStream_);
            videoInput_->updateStartTime(startTime_);
        }
    } catch (const std::exception& e) {
        videoInput_ = nullptr;
        JAMI_ERR("media player: %s open video input failed: %s", path_.c_str(), e.what());
    }
#endif

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
    case MediaDemuxer::Status::RestartRequired:
    default:
        break;
    }
}

void
MediaPlayer::emitInfo()
{
    std::map<std::string, std::string> info {{"duration", std::to_string(fileDuration_)},
                                             {"audio_stream", std::to_string(audioStream_)},
                                             {"video_stream", std::to_string(videoStream_)}};
    emitSignal<libjami::MediaPlayerSignal::FileOpened>(path_, info);
}

bool
MediaPlayer::isInputValid()
{
    return !path_.empty();
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
#ifdef ENABLE_VIDEO
    if (hasVideo()) {
        videoInput_->setPaused(paused_);
        videoInput_->updateStartTime(newTime);
    }
#endif
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

    int64_t currentTime = av_gettime();
    if (paused_){
        pauseInterval_ += currentTime - lastPausedTime_;
        lastPausedTime_ = currentTime;
    }

    startTime_ = currentTime - pauseInterval_ - time;
    if (hasAudio()) {
        audioInput_->setSeekTime(time);
        audioInput_->updateStartTime(startTime_);
    }
#ifdef ENABLE_VIDEO
    if (hasVideo()) {
        videoInput_->setSeekTime(time);
        videoInput_->updateStartTime(startTime_);
    }
#endif
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
#ifdef ENABLE_VIDEO
    if (hasVideo()) {
        videoInput_->updateStartTime(startTime_);
    }
#endif
    if (autoRestart_)
        pause(false);
}

void
MediaPlayer::flushMediaBuffers()
{
#ifdef ENABLE_VIDEO
    if (hasVideo()) {
        videoInput_->flushBuffers();
    }
#endif

    if (hasAudio()) {
        audioInput_->flushBuffers();
    }
}

const std::string&
MediaPlayer::getId() const
{
    return path_;
}

int64_t
MediaPlayer::getPlayerPosition() const
{
    if (paused_) {
        return lastPausedTime_ - startTime_ - pauseInterval_;
    }
    return av_gettime() - startTime_ - pauseInterval_;
}

int64_t
MediaPlayer::getPlayerDuration() const
{
    return fileDuration_;
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
