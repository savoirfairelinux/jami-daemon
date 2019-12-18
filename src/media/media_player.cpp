/*
 *  Copyright (C) 2020 Savoir-faire Linux Inc.
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
#include "manager.h"
#include <string>
namespace jami {

static constexpr auto MS_PER_PACKET = std::chrono::milliseconds(20);

MediaPlayer::MediaPlayer(const std::string& path)
{
    static const std::string sep = DRing::Media::VideoProtocolPrefix::SEPARATOR;
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
    videoInput_ = jami::getVideoInput(id_,
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
            Manager::instance().startAudioPlayback();
            audioInput_->configureFilePlayback(path_, demuxer_, audioStream_);
            audioInput_->updateStartTime(startTime_);
        }
    } catch (const std::exception& e) {
        JAMI_ERR("media player: %s open audio input failed: %s",
                 path_.c_str(), e.what());
    }
    try {
        videoStream_ = demuxer_->selectStream(AVMEDIA_TYPE_VIDEO);
        if (hasVideo()) {
            videoInput_->setSink(id_);
            videoInput_->configureFilePlayback(path_, demuxer_, videoStream_);
            if (!hasAudio()) {
                videoInput_->emulateRate();
            }
            videoInput_->updateStartTime(startTime_);
            muteAudio(true);
        }
    } catch (const std::exception& e) {
        videoInput_ = nullptr;
        JAMI_ERR("media player: %s open video input failed: %s",
                 path_.c_str(), e.what());
    }

    if (hasVideo() && hasAudio()) {
        audioInput_->setStreamSynk([this](int64_t time) -> void {
            videoInput_->getFramesBeforeTime(time);
        });
    }

    fileDuration_ = demuxer_->getDuration();
    emitInfo();
    return true;
}

void
MediaPlayer::emitInfo()
{
    std::map<std::string, std::string> info {
        {"duration", std::to_string(fileDuration_)},
        {"audio_stream", std::to_string(audioStream_)},
        {"video_stream", std::to_string(videoStream_)}
    };
    emitSignal<DRing::MediaPlayerSignal::FileOpened>(id_, info);
}

bool
MediaPlayer::inputValid() {
    return !id_.empty();
}

void
MediaPlayer::reedFile() {
    if (!demuxer_)
        return;
    readingFile = true;
    auto ret = MediaDemuxer::Status::Success;
    while (!paused_ && ret != MediaDemuxer::Status::EndOfFile) {
        if (flushBuffers) {
            flushMediaBuffers();
            flushBuffers = false;
        }
        ret = demuxer_->decode();
    }
    readingFile = false;
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
        if (!readingFile) {
            reedFile();
        }
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
    if (!readingFile) {
        reedFile();
    }
    startTime_ = av_gettime() - pauseInterval_ - time;
    flushBuffers = true;
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
MediaPlayer::getId()
{
    return id_;
}

int64_t
MediaPlayer::getPlayerPosition()
{
    if (paused_ ) {
        return lastPausedTime_ - startTime_ - pauseInterval_;
    }
    return av_gettime() - startTime_ - pauseInterval_;
}

bool
MediaPlayer::isPaused()
{
    return paused_;
}

}// namespace jami

