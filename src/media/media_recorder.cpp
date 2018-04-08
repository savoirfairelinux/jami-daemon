/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "fileutils.h"
#include "logger.h"
#include "media_io_handle.h"
#include "media_recorder.h"
#include "system_codec_container.h"

extern "C" {
#include <libavutil/frame.h>
}

#include <algorithm>
#include <sstream>
#include <sys/types.h>
#include <time.h>

namespace ring {

static std::string
createTimestamp()
{
    time_t rawtime = time(NULL);
    struct tm * timeinfo = localtime(&rawtime);
    std::stringstream out;

    // DATE
    out << timeinfo->tm_year + 1900;
    if (timeinfo->tm_mon < 9) // prefix jan-sep with 0
        out << 0;
    out << timeinfo->tm_mon + 1; // tm_mon is 0 based
    if (timeinfo->tm_mday < 10) // make sure there's 2 digits
        out << 0;
    out << timeinfo->tm_mday;

    out << '-';

    // HOUR
    if (timeinfo->tm_hour < 10) // make sure there's 2 digits
        out << 0;
    out << timeinfo->tm_hour;
    if (timeinfo->tm_min < 10) // make sure there's 2 digits
        out << 0;
    out << timeinfo->tm_min;
    if (timeinfo->tm_sec < 10) // make sure there's 2 digits
        out << 0;
    out << timeinfo->tm_sec;

    return out.str();
}

MediaRecorder::MediaRecorder()
{}

MediaRecorder::~MediaRecorder()
{
    if (isRecording_)
        flush();
}

std::string
MediaRecorder::getFilename() const
{
    if (audioOnly_)
        return dir_ + filename_ + ".ogg";
    else
        return dir_ + filename_ + ".mkv";
}

void
MediaRecorder::audioOnly(bool audioOnly)
{
    audioOnly_ = audioOnly;
}

void
MediaRecorder::setRecordingPath(const std::string& dir)
{
    if (!dir.empty() && fileutils::isDirectory(dir))
        dir_ = dir;
    else
        dir_ = fileutils::get_home_dir();
    if (dir_.back() != DIR_SEPARATOR_CH)
        dir_ = dir_ + DIR_SEPARATOR_CH;
    RING_DBG() << "Recording will be saved in '" << dir_ << "'";
}

void
MediaRecorder::incrementStreams(int n)
{
    nbStreams_ += n;
}

bool
MediaRecorder::isRecording() const
{
    return isRecording_;
}

bool
MediaRecorder::toggleRecording()
{
    if (isRecording_) {
        stopRecording();
    } else {
        startRecording();
    }
    return isRecording_;
}

int
MediaRecorder::startRecording()
{
    filename_ = createTimestamp();

    encoder_.reset();

    std::unique_ptr<MediaIOHandle> ioHandle;
    std::map<std::string, std::string> options;

    encoder_->openFileOutput(getFilename(), options);
    encoder_->setIOContext(ioHandle);
    RING_DBG() << "Start recording '" << getFilename() << "'";

    isRecording_ = true;
    return 0;
}

void
MediaRecorder::stopRecording()
{
    if (isRecording_) {
        RING_DBG() << "Stop recording '" << getFilename() << "'";
        flush();
    }
    isRecording_ = false;
}

int
MediaRecorder::addStream(bool fromPeer, bool isVideo)
{
    if (audioOnly_ && isVideo)
        return -1;

    auto key = std::make_pair(fromPeer, isVideo);
    if (streamMap_.find(key) != streamMap_.end()) {
        RING_WARN() << "Could not create stream: key already exists";
        return -1;
    }

    // TODO manage packet side data if necessary

    int streamIdx = -1;
    if (isVideo) {
        auto videoCodec = std::static_pointer_cast<ring::SystemVideoCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("VP8", ring::MEDIA_VIDEO));
        streamIdx = encoder_->addStream(*videoCodec.get());
        ++nbVideoStreams_;
    } else {
        auto audioCodec = std::static_pointer_cast<ring::SystemVideoCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("opus", ring::MEDIA_AUDIO));
        streamIdx = encoder_->addStream(*audioCodec.get());
        ++nbAudioStreams_;
    }

    if (streamIdx < 0) {
        RING_ERR() << "Failed to add stream to encoder";
        return -1;
    }

    streamMap_[key] = streamIdx;
    nextTimestamp_[streamIdx] = 0;

    if (encoder_->getStreamCount() == nbStreams_) {
        RING_DBG() << "Recording initialized";
        encoder_->startIO();
        isReady_ = true;
    }

    return 0;
}

int
MediaRecorder::recordData(AVFrame* frame, bool fromPeer, bool isVideo)
{
    if (!isRecording_)
        return 0;

    if (!isReady_ || nbStreams_ != encoder_->getStreamCount()) {
        RING_ERR() << "Recorder not ready";
        return -1;
    }

    auto key = std::make_pair(fromPeer, isVideo);
    auto it = streamMap_.find(key);
    if (it == streamMap_.end()) {
        RING_WARN() << (fromPeer ? "Remote " : "Local ") << (isVideo ? "video" : "audio")
            << " stream not found";
        return -1;
    }

    int streamIdx = streamMap_[key];
    if (!isVideo)
        nextTimestamp_[streamIdx] += frame->nb_samples;
    else
        nextTimestamp_[streamIdx]++;

    int ret = 0;
    try {
        ret = encoder_->encode(frame, streamIdx);
    } catch (const MediaEncoderException& e) {
        RING_ERR() << "MediaEncoderException: " << e.what();
        ret = -1;
    }
    av_frame_free(&frame);
    return ret;
}

int
MediaRecorder::flush()
{
    if (!isRecording_ || encoder_->getStreamCount() <= 0)
        return 0;

    encoder_->flush();

    return 0;
}

} // namespace ring
