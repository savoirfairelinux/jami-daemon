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
#include <iomanip>
#include <sstream>
#include <sys/types.h>
#include <ctime>

namespace ring {

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
    if (isRecording_)
        flush();
    if (loop_.isRunning())
        loop_.join();
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
MediaRecorder::setMetadata(const std::string& title, const std::string& desc)
{
    title_ = title;
    description_ = desc;
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
    nbExpectedStreams_ += n;
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
    std::time_t t = std::time(nullptr);
    startTime_ = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&startTime_, "%Y%m%d-%H%M%S");
    filename_ = ss.str();

    if (!frames_.empty()) {
        RING_WARN() << "Frame queue not empty at beginning of recording, frames will be lost";
        while (!frames_.empty())
            frames_.pop();
    }

    if (!loop_.isRunning())
        loop_.start();

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
MediaRecorder::addStream(bool isVideo, bool fromPeer, MediaStream ms)
{
    if (audioOnly_ && isVideo) {
        RING_ERR() << "Trying to add video stream to audio only recording";
        return -1;
    }

    // overwrite stream name for simplicity's sake
    std::string streamName;
    if (isVideo) {
        ms.name = (fromPeer ? "v:main" : "v:overlay");
        ++nbReceivedVideoStreams_;
    } else {
        ms.name = (fromPeer ? "a:1" : "a:2");
        ++nbReceivedAudioStreams_;
    }
    streams_[isVideo][fromPeer] = ms;

    // wait until all streams are ready before writing to the file
    if (nbExpectedStreams_ != nbReceivedAudioStreams_ + nbReceivedVideoStreams_)
        return 0;
    else
        return initRecord();
}

int
MediaRecorder::recordData(AVFrame* frame, bool isVideo, bool fromPeer)
{
    if (!isRecording_ || !isReady_)
        return 0;

    // save a copy of the frame, will be filtered/encoded in another thread
    AVFrame* input = av_frame_clone(frame);
    frames_.emplace(input, isVideo, fromPeer);
    return 0;
}

int
MediaRecorder::initRecord()
{
    std::lock_guard<std::mutex> lk(mutex_);

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
    if (nbReceivedVideoStreams_ > 0) {
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
    if (nbReceivedAudioStreams_ > 0) {
        const MediaStream& audioStream = setupAudioOutput();
        if (audioStream.format < 0) {
            RING_ERR() << "Could not retrieve audio recorder stream properties";
            return -1;
        }
        encoderOptions["sample_rate"] = std::to_string(audioStream.sampleRate);
        encoderOptions["channels"] = std::to_string(audioStream.nbChannels);
    }

    encoder_->openFileOutput(getFilename(), encoderOptions);

    if (nbReceivedVideoStreams_ > 0) {
        auto videoCodec = std::static_pointer_cast<ring::SystemVideoCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("VP8", ring::MEDIA_VIDEO));
        videoIdx_ = encoder_->addStream(*videoCodec.get());
        if (videoIdx_ < 0) {
            RING_ERR() << "Failed to add video stream to encoder";
            return -1;
        }
    }

    if (nbReceivedAudioStreams_ > 0) {
        auto audioCodec = std::static_pointer_cast<ring::SystemAudioCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("opus", ring::MEDIA_AUDIO));
        audioIdx_ = encoder_->addStream(*audioCodec.get());
        if (audioIdx_ < 0) {
            RING_ERR() << "Failed to add audio stream to encoder";
            return -1;
        }
    }

    // ready to start recording if audio stream index and video stream index are valid
    isReady_ = (nbReceivedAudioStreams_ > 0 && audioIdx_ >= 0)
        && (audioOnly_ || (nbReceivedVideoStreams_ > 0 && videoIdx_ >= 0));
    if (isReady_) {
        std::unique_ptr<MediaIOHandle> ioHandle;
        try {
            encoder_->setIOContext(ioHandle);
            encoder_->startIO();
        } catch (const MediaEncoderException& e) {
            RING_ERR() << "Could not start recorder: " << e.what();
            return -1;
        }
        RING_DBG() << "Recording initialized";
        return 0;
    } else {
        RING_ERR() << "Something went wrong when initializing the recorder";
        return -1;
    }
}

MediaStream
MediaRecorder::setupVideoOutput()
{
    MediaStream encoderStream;
    const MediaStream& peer = streams_[true][true];
    const MediaStream& local = streams_[true][false];

    switch (nbReceivedVideoStreams_) {
    case 1: // use a stream with a valid size
        if (peer.width > 0 && peer.height > 0)
            encoderStream = peer;
        else if (local.width > 0 && local.height > 0)
            encoderStream = local;
        else
            encoderStream.format = -1; // invalidate stream
        break;
    case 2: // overlay local video over peer video
        videoFilter_.reset(new MediaFilter);
        if (videoFilter_->initialize(buildVideoFilter(),
                (std::vector<MediaStream>){peer, local}) < 0) {
            RING_ERR() << "Failed to initialize video filter";
            encoderStream.format = -1; // invalidate stream
        } else {
            encoderStream = videoFilter_->getOutputParams();
        }
        break;
    default:
        RING_ERR() << "Recording more than 2 video streams is not supported";
        encoderStream.format = -1; // invalidate stream
    }

    RING_DBG() << "Video recorder '"
        << (encoderStream.name.empty() ? "(null)" : encoderStream.name)
        << "' properties: "
        << av_get_pix_fmt_name(static_cast<AVPixelFormat>(encoderStream.format)) << ", "
        << encoderStream.width << "x" << encoderStream.height << ", "
        << encoderStream.frameRate << " fps";
    return encoderStream;
}

std::string
MediaRecorder::buildVideoFilter()
{
    std::stringstream v;

    const MediaStream& p = streams_[true][true];
    const MediaStream& l = streams_[true][false];

    const constexpr int minHeight = 720;
    const auto newFps = std::max(p.frameRate, l.frameRate);
    const bool needScale = (p.height < minHeight);
    const int newHeight = (needScale ? minHeight : p.height);

    // NOTE -2 means preserve aspect ratio and have the new number be even
    if (needScale)
        v << "[v:main] fps=" << newFps << ", scale=-2:" << newHeight << " [v:m]; ";
    else
        v << "[v:main] fps=" << newFps << " [v:m]; ";

    v << "[v:overlay] fps=" << newFps << ", scale=-2:" << newHeight / 5 << " [v:o]; ";

    v << "[v:m] [v:o] overlay=main_w-overlay_w-10:main_h-overlay_h-10"
        << ", format=pix_fmts=yuv420p";

    return v.str();
}

MediaStream
MediaRecorder::setupAudioOutput()
{
    MediaStream encoderStream;
    const MediaStream& peer = streams_[false][true];
    const MediaStream& local = streams_[false][false];
    std::stringstream aFilter;

    switch (nbReceivedAudioStreams_) {
    case 1: // use a stream with a valid sample rate and channel count
        if (peer.sampleRate > 0 && peer.nbChannels > 0)
            encoderStream = peer;
        else if (local.sampleRate > 0 && local.nbChannels > 0)
            encoderStream = local;
        else
            encoderStream.format = -1; // invalidate stream
        break;
    case 2: // mix both audio streams
        audioFilter_.reset(new MediaFilter);
        // resample to common audio format, so any player can play the file
        aFilter << "[a:1] [a:2] amix, aresample=osr=48000:ocl=stereo:osf=s16";
        if (audioFilter_->initialize(aFilter.str(),
                (std::vector<MediaStream>){peer, local}) < 0) {
            RING_ERR() << "Failed to initialize audio filter";
            encoderStream.format = -1; // invalidate stream
        } else {
            encoderStream = audioFilter_->getOutputParams();
        }
        break;
    default:
        RING_ERR() << "Recording more than 2 audio streams is not supported";
        encoderStream.format = -1; // invalidate stream
        break;
    }

    RING_DBG() << "Audio recorder '"
        << (encoderStream.name.empty() ? "(null)" : encoderStream.name)
        << "' properties: "
        << av_get_sample_fmt_name(static_cast<AVSampleFormat>(encoderStream.format)) << ", "
        << encoderStream.sampleRate << " Hz, "
        << encoderStream.nbChannels << " channels";
    return encoderStream;
}

void
MediaRecorder::emptyFilterGraph()
{
    AVFrame* output;
    if (videoIdx_ >= 0)
        while ((output = videoFilter_->readOutput()))
            sendToEncoder(output, videoIdx_);
    if (audioIdx_ >= 0)
        while ((output = audioFilter_->readOutput()))
            sendToEncoder(output, audioIdx_);
}

int
MediaRecorder::sendToEncoder(AVFrame* frame, int streamIdx)
{
    try {
        encoder_->encode(frame, streamIdx);
    } catch (const MediaEncoderException& e) {
        RING_ERR() << "MediaEncoderException: " << e.what();
        av_frame_unref(frame);
        return -1;
    }
    av_frame_unref(frame);
    return 0;
}

int
MediaRecorder::flush()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (!isRecording_ || encoder_->getStreamCount() <= 0)
        return 0;

    emptyFilterGraph();
    encoder_->flush();

    return 0;
}

void
MediaRecorder::process()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (!isRecording_ || !isReady_)
        return;

    while (!frames_.empty()) {
        auto f = frames_.front();
        bool isVideo = f.isVideo;
        bool fromPeer = f.fromPeer;
        AVFrame* input = f.frame;
        frames_.pop();

        int streamIdx = (isVideo ? videoIdx_ : audioIdx_);
        auto filter = (isVideo ? videoFilter_.get() : audioFilter_.get());
        if (streamIdx < 0 || !filter) {
            RING_ERR() << "Specified stream is invalid: "
                << (fromPeer ? "remote " : "local ") << (isVideo ? "video" : "audio");
            av_frame_free(&input);
            continue;
        }

        // get filter input name if frame needs filtering
        std::string inputName;
        if (isVideo && nbReceivedVideoStreams_ == 2)
            inputName = (fromPeer ? "v:main" : "v:overlay");
        if (!isVideo && nbReceivedAudioStreams_ == 2)
            inputName = (fromPeer ? "a:1" : "a:2");

        // new reference because we are changing the timestamp
        const MediaStream& ms = streams_[isVideo][fromPeer];
        // stream has to start at 0
        input->pts = input->pts - ms.firstTimestamp;

        if (inputName.empty()) { // #nofilters
            if (sendToEncoder(input, streamIdx) < 0) {
                RING_ERR() << "Filed to encode frame";
                av_frame_free(&input);
                continue;
            }
        }

        // empty filter graph output before sending more frames
        emptyFilterGraph();

        filter->feedInput(input, inputName);
        av_frame_free(&input);
    }
}

} // namespace ring
