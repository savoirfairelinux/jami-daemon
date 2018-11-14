/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
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
#include "client/ring_signal.h"
#include "fileutils.h"
#include "logger.h"
#include "media_io_handle.h"
#include "media_recorder.h"
#include "system_codec_container.h"
#include "thread_pool.h"

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
{}

MediaRecorder::~MediaRecorder()
{}

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
        emitSignal<DRing::CallSignal::RecordPlaybackStopped>(getPath());
    }
}

Observer<std::shared_ptr<MediaFrame>>*
MediaRecorder::addStream(const MediaStream& ms)
{
    if (audioOnly_ && ms.isVideo) {
        RING_ERR() << "Trying to add video stream to audio only recording";
        return nullptr;
    }

    auto ptr = std::make_unique<StreamObserver>(ms, [this, ms](const std::shared_ptr<MediaFrame>& frame) {
        onFrame(ms.name, frame);
    });
    auto p = streams_.insert(std::make_pair(ms.name, std::move(ptr)));
    if (p.second) {
        RING_DBG() << "Recorder input #" << streams_.size() << ": " << ms;
        if (ms.isVideo)
            hasVideo_ = true;
        else
            hasAudio_ = true;
        return p.first->second.get();
    } else {
        RING_WARN() << "Recorder already has '" << ms.name << "' as input";
        return p.first->second.get();
    }
}

Observer<std::shared_ptr<MediaFrame>>*
MediaRecorder::getStream(const std::string& name) const
{
    const auto it = streams_.find(name);
    if (it != streams_.cend())
        return it->second.get();
    return nullptr;
}

void
MediaRecorder::onFrame(const std::string& name, const std::shared_ptr<MediaFrame>& frame)
{
    if (!isRecording_)
        return;

    // copy frame to not mess with the original frame's pts (does not actually copy frame data)
    MediaFrame clone;
    const auto& ms = streams_[name]->info;
    clone.copyFrom(*frame);
    clone.pointer()->pts -= ms.firstTimestamp;
    if (ms.isVideo)
        videoFilter_->feedInput(clone.pointer(), name);
    else
        audioFilter_->feedInput(clone.pointer(), name);
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
        description_ = "Recorded with Jami https://jami.net";
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
    ThreadPool::instance().run([rec = shared_from_this()] {
        while (rec->isRecording()) {
            rec->filterAndEncode(rec->videoFilter_.get(), rec->videoIdx_);
            rec->filterAndEncode(rec->audioFilter_.get(), rec->audioIdx_);
        }
        rec->flush();
        rec->reset(); // allows recorder to be reused in same call
    });
    return 0;
}

MediaStream
MediaRecorder::setupVideoOutput()
{
    MediaStream encoderStream, peer, local;
    auto it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair){
        return pair.second->info.isVideo && pair.second->info.name.find("remote") != std::string::npos;
    });

    if (it != streams_.end()) {
        peer = it->second->info;
    }

    it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair){
        return pair.second->info.isVideo && pair.second->info.name.find("local") != std::string::npos;
    });

    if (it != streams_.end()) {
        local = it->second->info;
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
        return !pair.second->info.isVideo && pair.second->info.name.find("remote") != std::string::npos;
    });

    if (it != streams_.end()) {
        peer = it->second->info;
    }

    it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair){
        return !pair.second->info.isVideo && pair.second->info.name.find("local") != std::string::npos;
    });

    if (it != streams_.end()) {
        local = it->second->info;
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
    std::lock_guard<std::mutex> lk(mutex_);
    if (videoFilter_)
        videoFilter_->flush();
    if (audioFilter_)
        audioFilter_->flush();
    filterAndEncode(videoFilter_.get(), videoIdx_);
    filterAndEncode(audioFilter_.get(), audioIdx_);
    encoder_->flush();
}

void
MediaRecorder::reset()
{
    streams_.clear();
    videoIdx_ = audioIdx_ = -1;
    audioOnly_ = false;
    videoFilter_.reset();
    audioFilter_.reset();
    encoder_.reset();
}

void
MediaRecorder::filterAndEncode(MediaFilter* filter, int streamIdx)
{
    if (filter && streamIdx >= 0) {
        while (auto frame = filter->readOutput()) {
            std::lock_guard<std::mutex> lk(mutex_);
            switch (encoder_->encode(frame, streamIdx)) {
                case MediaEncoder::Status::ReadError:
                    RING_ERR("fatal error, read failed");
                    break;
                case MediaEncoder::Status::EncodeError:
                    RING_ERR("fatal error, encode failed");
                    break;
                case MediaEncoder::Status::RestartRequired: {
                    // disable accel, reset encoder's AVCodecContext
#ifdef RING_ACCEL
                    encoder_->enableAccel(false);
#endif
                    auto videoCodec = std::static_pointer_cast<ring::SystemVideoCodecInfo>(
                        getSystemCodecContainer()->searchCodecByName("H264", ring::MEDIA_VIDEO));
                    encoder_->addStream(*videoCodec.get(),NULL);
                    break;
                }
                default:
                    break;
            }
            av_frame_free(&frame);
        }
    }
}

} // namespace ring
