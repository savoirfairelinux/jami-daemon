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
    encoder_.reset(new MediaEncoder);

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
MediaRecorder::addStream(bool fromPeer, bool isVideo, AVStream* stream)
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (audioOnly_ && isVideo)
        return -1;

    auto key = std::make_pair(fromPeer, isVideo);
    if (streamMap_.find(key) != streamMap_.end()) {
        RING_WARN() << "Could not create stream (" << fromPeer << "," << isVideo << "): key already exists";
        return -1;
    }

    if (isVideo) {
        filterParams_[key] = MediaFilterParameters(stream->codecpar->format,
                                                   rational<int>(stream->time_base.num, stream->time_base.den),
                                                   stream->codecpar->width,
                                                   stream->codecpar->height,
                                                   rational<int>(stream->codecpar->sample_aspect_ratio.num,
                                                                stream->codecpar->sample_aspect_ratio.den),
                                                   rational<int>(stream->r_frame_rate.num,
                                                                 stream->r_frame_rate.den));
        ++nbVideoStreams_;
    } else {
        filterParams_[key] = MediaFilterParameters(stream->codecpar->format,
                                                   rational<int>(stream->time_base.num, stream->time_base.den),
                                                   stream->codecpar->sample_rate,
                                                   stream->codecpar->channels);
        ++nbAudioStreams_;
    }

    // wait until all filter params are gotten
    // nbStreams_ is incremented early, while the other two are incremented above
    if (nbStreams_ != nbAudioStreams_ + nbVideoStreams_)
        return -1;

    int peerWidth = filterParams_[std::make_pair(true, true)].width;
    int peerHeight = filterParams_[std::make_pair(true, true)].height;
    std::map<std::string, std::string> options;
    options["sample_rate"] = "48000";
    options["channels"] = "2";
    options["width"] = std::to_string(peerWidth);
    options["height"] = std::to_string(peerHeight);

    if (!audioFilter_) {
        std::string filterDesc = "[a:local] [a:peer] amix";
        std::vector<MediaFilterParameters> vec; // order is important
        vec.push_back(filterParams_[std::make_pair(false, false)]); // local audio
        vec.push_back(filterParams_[std::make_pair(true, false)]); // peer audio
        audioFilter_.reset(new MediaFilter);
        audioFilter_->initialize(filterDesc, vec);
    }

    if (!videoFilter_) {
        std::stringstream filterDesc;
        filterDesc << "[v:local] scale=" << peerWidth / 4 << ":" << peerHeight / 4 << " [v:scaled]; "
            << "[v:peer] [v:scaled] overlay=main_w-overlay_w-10:main_h-overlay_h-10";
        std::vector<MediaFilterParameters> vec; // order is important
        vec.push_back(filterParams_[std::make_pair(false, true)]); // local video
        vec.push_back(filterParams_[std::make_pair(true, true)]); // peer video
        videoFilter_.reset(new MediaFilter);
        videoFilter_->initialize(filterDesc.str(), vec);
    }

    encoder_->openFileOutput(getFilename(), options);

    // recorder has 1 audio stream and 1 video stream
    int streamIdx = -1;
    if (videoIdx_ < 0) {
        auto videoCodec = std::static_pointer_cast<ring::SystemVideoCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("VP8", ring::MEDIA_VIDEO));
        videoIdx_ = encoder_->addStream(*videoCodec.get());
        if (videoIdx_ < 0)
            RING_ERR() << "Failed to add video stream to encoder";
        streamIdx = videoIdx_;
    }

    if (audioIdx_ < 0) {
        auto audioCodec = std::static_pointer_cast<ring::SystemAudioCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("opus", ring::MEDIA_AUDIO));
        audioIdx_ = encoder_->addStream(*audioCodec.get());
        if (audioIdx_ < 0)
            RING_ERR() << "Failed to add stream to encoder";
        streamIdx = audioIdx_;
    }

    streamMap_[key] = streamIdx;
    nextTimestamp_[streamIdx] = 0;

    if (encoder_->getStreamCount() == 2) {
        std::unique_ptr<MediaIOHandle> ioHandle;
        try {
            encoder_->setIOContext(ioHandle);
            encoder_->startIO();
        } catch (const MediaEncoderException& e) {
            RING_ERR() << "Could not start recorder: " << e.what();
            return -1;
        }
        isReady_ = true;
        RING_DBG() << "Recording initialized";
    }

    return 0;
}

int
MediaRecorder::recordData(AVFrame* frame, bool fromPeer, bool isVideo)
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (!isRecording_)
        return 0;

    if (!isReady_) {
        return 0; // not an error, we expect to receive frames before all streams are setup
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

    // apply filters
    if (isVideo) {
        std::string filterName = (fromPeer ? "v:peer" : "v:local");
        videoFilter_->feedInput(frame, filterName);
        av_frame_unref(frame);
    } else {
        std::string filterName = (fromPeer ? "a:peer" : "a:local");
        audioFilter_->feedInput(frame, filterName);
        av_frame_unref(frame);
    }

    if (!frame) {
        if (isVideo) {
            frame = videoFilter_->readOutput();
        } else {
            frame = audioFilter_->readOutput();
        }
    }

    if (!frame) {
        return 0;
    }

    int ret = 0;
    try {
        if (frame)
            ret = encoder_->encode(frame, streamIdx);
    } catch (const MediaEncoderException& e) {
        RING_ERR() << "MediaEncoderException: " << e.what();
        ret = -1;
    }
    av_frame_unref(frame);
    return ret;
}

int
MediaRecorder::flush()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (!isRecording_ || encoder_->getStreamCount() <= 0)
        return 0;

    encoder_->flush();

    return 0;
}

} // namespace ring
