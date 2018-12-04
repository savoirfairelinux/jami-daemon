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
#include "audio/audio_input.h"
#include "audio/audio_receive_thread.h"
#include "audio/audio_sender.h"
#include "client/ring_signal.h"
#include "fileutils.h"
#include "logger.h"
#include "media_io_handle.h"
#include "media_recorder.h"
#include "system_codec_container.h"
#include "video/video_input.h"
#include "video/video_receive_thread.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <sys/types.h>
#include <ctime>

namespace ring {

// Replaces every occurrence of @from with @to in @str
static std::string
replaceAll(const std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
        return str;
    std::string copy(str);
    size_t startPos = 0;
    while ((startPos = str.find(from, startPos)) != std::string::npos) {
        copy.replace(startPos, from.length(), to);
        startPos += to.length();
    }
    return copy;
}

MediaRecorder::MediaRecorder()
    : loop_([]{ return true;},
            [this]{ process(); },
            []{})
{}

MediaRecorder::~MediaRecorder()
{
    if (loop_.isRunning())
        loop_.join();
    if (isRecording_)
        flush();
}

bool
MediaRecorder::isRecording() const
{
    return isRecording_;
}

std::string
MediaRecorder::getPath() const
{
    if (audioOnly_)
        return path_ + ".ogg";
    else
        return path_ + ".webm";
}

void
MediaRecorder::audioOnly(bool audioOnly)
{
    audioOnly_ = audioOnly;
}

void
MediaRecorder::setPath(const std::string& path)
{
    path_ = path;
}

void
MediaRecorder::setMetadata(const std::string& title, const std::string& desc)
{
    title_ = title;
    description_ = desc;
}

int
MediaRecorder::startRecording()
{
    std::time_t t = std::time(nullptr);
    startTime_ = *std::localtime(&t);

    encoder_.reset(new MediaEncoder);

    RING_DBG() << "Start recording '" << getPath() << "'";
    if (initRecord() >= 0)
        isRecording_ = true;
    return 0;
}

void
MediaRecorder::stopRecording()
{
    if (isRecording_) {
        RING_DBG() << "Stop recording '" << getPath() << "'";
        isRecording_ = false;
        loop_.join();
        flush();
        emitSignal<DRing::CallSignal::RecordPlaybackStopped>(getPath());
    }
    streams_.clear();
    videoIdx_ = audioIdx_ = -1;
    isRecording_ = false;
    audioOnly_ = false;
    videoFilter_.reset();
    audioFilter_.reset();
    encoder_.reset();
}

int
MediaRecorder::addStream(const MediaStream& ms)
{
    if (audioOnly_ && ms.isVideo) {
        RING_ERR() << "Trying to add video stream to audio only recording";
        return -1;
    }

    if (streams_.insert(std::make_pair(ms.name, ms)).second) {
        RING_DBG() << "Recorder input #" << streams_.size() << ": " << ms;
        if (ms.isVideo)
            hasVideo_ = true;
        else
            hasAudio_ = true;
        return 0;
    } else {
        RING_ERR() << "Could not add stream '" << ms.name << "' to record";
        return -1;
    }
}

void
MediaRecorder::update(Observable<std::shared_ptr<AudioFrame>>* ob, const std::shared_ptr<AudioFrame>& a)
{
    std::string name;
    if (dynamic_cast<AudioReceiveThread*>(ob))
        name = "a:remote";
    else // ob is of type AudioInput*
        name = "a:local";
    // copy frame to not mess with the original frame's pts
    AudioFrame clone;
    clone.copyFrom(*a);
    clone.pointer()->pts -= streams_[name].firstTimestamp;
    audioFilter_->feedInput(clone.pointer(), name);
    loop_.interrupt();
}

void MediaRecorder::update(Observable<std::shared_ptr<VideoFrame>>* ob, const std::shared_ptr<VideoFrame>& v)
{
    std::string name;
    if (dynamic_cast<video::VideoReceiveThread*>(ob))
        name = "v:remote";
    else // ob is of type VideoInput*
        name = "v:local";
    // copy frame to not mess with the original frame's pts
    VideoFrame clone;
    clone.copyFrom(*v);
    clone.pointer()->pts -= streams_[name].firstTimestamp;
    videoFilter_->feedInput(clone.pointer(), name);
    loop_.interrupt();
}

int
MediaRecorder::initRecord()
{
    // need to get encoder parameters before calling openFileOutput
    // openFileOutput needs to be called before adding any streams

    std::map<std::string, std::string> encoderOptions;

    std::stringstream timestampString;
    timestampString << std::put_time(&startTime_, "%Y-%m-%d %H:%M:%S");

    if (title_.empty()) {
        std::stringstream ss;
        ss << "Conversation at %TIMESTAMP";
        title_ = ss.str();
    }
    title_ = replaceAll(title_, "%TIMESTAMP", timestampString.str());
    encoderOptions["title"] = title_;

    if (description_.empty()) {
        description_ = "Recorded with Ring https://ring.cx";
    }
    description_ = replaceAll(description_, "%TIMESTAMP", timestampString.str());
    encoderOptions["description"] = description_;

    videoFilter_.reset();
    if (hasVideo_) {
        const MediaStream& videoStream = setupVideoOutput();
        if (videoStream.format < 0) {
            RING_ERR() << "Could not retrieve video recorder stream properties";
            return -1;
        }
        encoderOptions["width"] = std::to_string(videoStream.width);
        encoderOptions["height"] = std::to_string(videoStream.height);
        std::stringstream fps;
        fps << videoStream.frameRate;
        encoderOptions["framerate"] = fps.str();
    }

    audioFilter_.reset();
    if (hasAudio_) {
        const MediaStream& audioStream = setupAudioOutput();
        if (audioStream.format < 0) {
            RING_ERR() << "Could not retrieve audio recorder stream properties";
            return -1;
        }
        encoderOptions["sample_rate"] = std::to_string(audioStream.sampleRate);
        encoderOptions["channels"] = std::to_string(audioStream.nbChannels);
    }

    encoder_->openFileOutput(getPath(), encoderOptions);

    if (hasVideo_) {
        auto videoCodec = std::static_pointer_cast<ring::SystemVideoCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("VP8", ring::MEDIA_VIDEO));
        videoIdx_ = encoder_->addStream(*videoCodec.get());
        if (videoIdx_ < 0) {
            RING_ERR() << "Failed to add video stream to encoder";
            return -1;
        }
    }

    if (hasAudio_) {
        auto audioCodec = std::static_pointer_cast<ring::SystemAudioCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("opus", ring::MEDIA_AUDIO));
        audioIdx_ = encoder_->addStream(*audioCodec.get());
        if (audioIdx_ < 0) {
            RING_ERR() << "Failed to add audio stream to encoder";
            return -1;
        }
    }

    try {
        std::unique_ptr<MediaIOHandle> ioHandle;
        encoder_->setIOContext(ioHandle);
        encoder_->startIO();
    } catch (const MediaEncoderException& e) {
        RING_ERR() << "Could not start recorder: " << e.what();
        return -1;
    }

    RING_DBG() << "Recording initialized";
    loop_.start();
    return 0;
}

MediaStream
MediaRecorder::setupVideoOutput()
{
    MediaStream encoderStream, peer, local;
    auto it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair){
        return pair.second.isVideo && pair.second.name.find("remote") != std::string::npos;
    });

    if (it != streams_.end()) {
        peer = it->second;
    }

    it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair){
        return pair.second.isVideo && pair.second.name.find("local") != std::string::npos;
    });

    if (it != streams_.end()) {
        local = it->second;
    }

    // vp8 supports only yuv420p
    videoFilter_.reset(new MediaFilter);
    int ret = -1;
    int streams = peer.isValid() + local.isValid();
    switch (streams) {
    case 1:
    {
        auto inputStream = peer.isValid() ? peer : local;
        ret = videoFilter_->initialize(buildVideoFilter({}, inputStream), {inputStream});
        break;
    }
    case 2: // overlay local video over peer video
        ret = videoFilter_->initialize(buildVideoFilter({peer}, local), {peer, local});
        break;
    default:
        RING_ERR() << "Recording more than 2 video streams is not supported";
        break;
    }

    if (ret >= 0) {
        encoderStream = videoFilter_->getOutputParams();
        RING_DBG() << "Recorder output: " << encoderStream;
    } else {
        RING_ERR() << "Failed to initialize video filter";
    }

    return encoderStream;
}

std::string
MediaRecorder::buildVideoFilter(const std::vector<MediaStream>& peers, const MediaStream& local) const
{
    std::stringstream v;

    switch (peers.size()) {
    case 0:
        v << "[" << local.name << "] fps=30, format=pix_fmts=yuv420p";
        break;
    case 1:
        {
            auto p = peers[0];
            const constexpr int minHeight = 720;
            const auto newFps = std::max(p.frameRate, local.frameRate);
            const bool needScale = (p.height < minHeight);
            const int newHeight = (needScale ? minHeight : p.height);

            // NOTE -2 means preserve aspect ratio and have the new number be even
            if (needScale)
                v << "[" << p.name << "] fps=" << newFps << ", scale=-2:" << newHeight << " [v:m]; ";
            else
                v << "[" << p.name << "] fps=" << newFps << " [v:m]; ";

            v << "[" << local.name << "] fps=" << newFps << ", scale=-2:" << newHeight / 5 << " [v:o]; ";

            v << "[v:m] [v:o] overlay=main_w-overlay_w:main_h-overlay_h"
                << ", format=pix_fmts=yuv420p";
        }
        break;
    default:
        RING_ERR() << "Video recordings with more than 2 video streams are not supported";
        break;
    }

    return v.str();
}

MediaStream
MediaRecorder::setupAudioOutput()
{
    MediaStream encoderStream, peer, local;
    auto it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair){
        return !pair.second.isVideo && pair.second.name.find("remote") != std::string::npos;
    });

    if (it != streams_.end()) {
        peer = it->second;
    }

    it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair){
        return !pair.second.isVideo && pair.second.name.find("local") != std::string::npos;
    });

    if (it != streams_.end()) {
        local = it->second;
    }

    // resample to common audio format, so any player can play the file
    audioFilter_.reset(new MediaFilter);
    int ret = -1;
    int streams = peer.isValid() + local.isValid();
    switch (streams) {
    case 1:
    {
        auto inputStream = peer.isValid() ? peer : local;
        ret = audioFilter_->initialize(buildAudioFilter({}, inputStream), {inputStream});
        break;
    }
    case 2: // mix both audio streams
        ret = audioFilter_->initialize(buildAudioFilter({peer}, local), {peer, local});
        break;
    default:
        RING_ERR() << "Recording more than 2 audio streams is not supported";
        break;
    }

    if (ret >= 0) {
        encoderStream = audioFilter_->getOutputParams();
        RING_DBG() << "Recorder output: " << encoderStream;
    } else {
        RING_ERR() << "Failed to initialize audio filter";
    }

    return encoderStream;
}

std::string
MediaRecorder::buildAudioFilter(const std::vector<MediaStream>& peers, const MediaStream& local) const
{
    std::string baseFilter = "aresample=osr=48000:ocl=stereo:osf=s16";
    std::stringstream a;

    switch (peers.size()) {
    case 0:
        a << "[" << local.name << "] " << baseFilter;
        break;
    default:
        a << "[" << local.name << "] ";
        for (const auto& ms : peers)
            a << "[" << ms.name << "] ";
        a << " amix=inputs=" << peers.size() + (local.isValid() ? 1 : 0) << ", " << baseFilter;
        break;
    }

    return a.str();
}

void
MediaRecorder::flush()
{
    if (!isRecording_ || encoder_->getStreamCount() <= 0)
        return;

    std::lock_guard<std::mutex> lk(mutex_);
    encoder_->flush();
}

void
MediaRecorder::process()
{
    AVFrame* output;
    if (videoIdx_ >= 0 && videoFilter_)
        while ((output = videoFilter_->readOutput()))
            sendToEncoder(output, videoIdx_);
    if (audioIdx_ >= 0 && audioFilter_)
        while ((output = audioFilter_->readOutput()))
            sendToEncoder(output, audioIdx_);
}

void
MediaRecorder::sendToEncoder(AVFrame* frame, int streamIdx)
{
    try {
        std::lock_guard<std::mutex> lk(mutex_);
        encoder_->encode(frame, streamIdx);
    } catch (const MediaEncoderException& e) {
        RING_ERR() << "MediaEncoderException: " << e.what();
    }
    av_frame_free(&frame);
}

} // namespace ring
