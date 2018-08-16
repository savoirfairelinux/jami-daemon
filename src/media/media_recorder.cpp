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
#include "client/ring_signal.h"
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

static constexpr auto FRAME_DEQUEUE_INTERVAL = std::chrono::milliseconds(200);

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
MediaRecorder::getPath() const
{
    if (path_.empty()) {
        // FIXME deprecated code, will be removed once all clients transitioned to startRecording(path).
        if (audioOnly_)
            return dir_ + filename_ + ".ogg";
        else
            return dir_ + filename_ + ".webm";
    } else {
        if (audioOnly_)
            return path_ + ".ogg";
        else
            return path_ + ".webm";
    }
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
MediaRecorder::setPath(const std::string& path)
{
    if (!path.empty()) {
        path_ = path;
    }
    RING_DBG() << "Recording will be saved as '" << path_ << "'";
}

void
MediaRecorder::incrementExpectedStreams(int n)
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

    if (path_.empty()) {
        // FIXME deprecated code, will be removed once all clients transitioned to startRecording(path).
        std::stringstream ss;
        ss << std::put_time(&startTime_, "%Y%m%d-%H%M%S");
        filename_ = ss.str();
    }

    if (!frames_.empty()) {
        RING_WARN() << "Frame queue not empty at beginning of recording, frames will be lost";
        std::lock_guard<std::mutex> q(qLock_);
        while (!frames_.empty()) {
            auto f = frames_.front();
            av_frame_unref(f.frame);
            frames_.pop_front();
        }
    }

    encoder_.reset(new MediaEncoder);

    RING_DBG() << "Start recording '" << getPath() << "'";
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
    resetToDefaults();
}

int
MediaRecorder::addStream(const MediaStream& ms)
{
    if (audioOnly_ && ms.isVideo) {
        RING_ERR() << "Trying to add video stream to audio only recording";
        return -1;
    }

    if (streams_.insert(std::make_pair(ms.name, ms)).second) {
        RING_DBG() << "Recorder input #" << (nbReceivedAudioStreams_ + nbReceivedVideoStreams_) << ": " << ms;
    } else {
        RING_ERR() << "Could not add stream '" << ms.name << "' to record";
        return -1;
    }

    std::lock_guard<std::mutex> lk(mutex_);

    if (ms.isVideo)
        ++nbReceivedVideoStreams_;
    else
        ++nbReceivedAudioStreams_;

    // wait until all streams are ready before writing to the file
    if (nbExpectedStreams_ != nbReceivedAudioStreams_ + nbReceivedVideoStreams_)
        return 0;
    else
        return initRecord();
}

int
MediaRecorder::recordData(AVFrame* frame, const MediaStream& ms)
{
    // recorder may be recording, but not ready for the first frames
    if (!isRecording_)
        return 0;

    if (!isReady_ && streams_.find(ms.name) == streams_.end())
        if (addStream(ms) < 0)
            return -1;

    if (!isReady_ || !loop_.isRunning()) // check again in case initRecord was called
        return 0;

    const auto& params = streams_.at(ms.name);

    // save a copy of the frame, will be filtered/encoded in another thread
    AVFrame* input = av_frame_clone(frame);
    input->pts = input->pts - params.firstTimestamp; // stream has to start at 0
    bool fromPeer = params.name.find("remote") != std::string::npos;

    {
        std::lock_guard<std::mutex> q(qLock_);
        frames_.emplace_back(input, params.isVideo, fromPeer);
    }
    loop_.interrupt();
    return 0;
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

    encoder_->openFileOutput(getPath(), encoderOptions);

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
    bool audioIsReady = nbReceivedAudioStreams_ == 0 || (nbReceivedAudioStreams_ > 0 && audioIdx_ >= 0);
    bool videoIsReady = (audioOnly_ && nbReceivedVideoStreams_ == 0) || (nbReceivedVideoStreams_ > 0 && videoIdx_ >= 0);
    isReady_ = audioIsReady && videoIsReady;

    if (isReady_) {
        if (!loop_.isRunning())
            loop_.start();

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

    switch (nbReceivedVideoStreams_) {
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
        v << "[" << local.name << "] format=pix_fmts=yuv420p";
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

            v << "[v:m] [v:o] overlay=main_w-overlay_w-10:main_h-overlay_h-10"
                << ", format=pix_fmts=yuv420p";
        }
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

    switch (nbReceivedAudioStreams_) {
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
    case 1:
        a << "[" << peers[0].name << "] [" << local.name << "] amix, " << baseFilter;
    }

    return a.str();
}

void
MediaRecorder::emptyFilterGraph()
{
    AVFrame* output;
    if (videoIdx_ >= 0 && videoFilter_)
        while ((output = videoFilter_->readOutput()))
            sendToEncoder(output, videoIdx_);
    if (audioIdx_ >= 0 && audioFilter_)
        while ((output = audioFilter_->readOutput()))
            sendToEncoder(output, audioIdx_);
}

int
MediaRecorder::sendToEncoder(AVFrame* frame, int streamIdx)
{
    try {
        std::lock_guard<std::mutex> lk(mutex_);
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
    if (!isRecording_ || encoder_->getStreamCount() <= 0)
        return 0;

    std::lock_guard<std::mutex> lk(mutex_);
    encoder_->flush();

    return 0;
}

void
MediaRecorder::resetToDefaults()
{
    streams_.clear();
    videoIdx_ = audioIdx_ = -1;
    nbExpectedStreams_ = 0;
    nbReceivedVideoStreams_ = nbReceivedAudioStreams_ = 0;
    isRecording_ = false;
    isReady_ = false;
    audioOnly_ = false;
    videoFilter_.reset();
    audioFilter_.reset();
    encoder_.reset();
}

void
MediaRecorder::process()
{
    // wait until frames_ is not empty or until we are no longer recording
    loop_.wait([this]{ return !frames_.empty(); });

    // if we exited because we stopped recording, stop our thread
    if (loop_.isStopping())
        return;

    // else encode a frame
    RecordFrame recframe;
    {
        std::lock_guard<std::mutex> q(qLock_);
        if (!frames_.empty()) {
            recframe = frames_.front();
            frames_.pop_front();
        } else {
            return;
        }
    }

    AVFrame* input = recframe.frame;
    int streamIdx = (recframe.isVideo ? videoIdx_ : audioIdx_);
    auto filter = (recframe.isVideo ? videoFilter_.get() : audioFilter_.get());
    if (streamIdx < 0) {
        RING_ERR() << "Specified stream is invalid: "
            << (recframe.fromPeer ? "remote " : "local ")
            << (recframe.isVideo ? "video" : "audio");
        av_frame_unref(input);
        return;
    }

    auto it = std::find_if(streams_.cbegin(), streams_.cend(), [recframe](const auto& pair){
        return pair.second.isVideo == recframe.isVideo &&
            recframe.fromPeer == (pair.second.name.find("remote") != std::string::npos);
    });

    if (it == streams_.cend()) {
        RING_ERR() << "Specified stream could not be found: "
            << (recframe.fromPeer ? "remote " : "local ")
            << (recframe.isVideo ? "video" : "audio");
        av_frame_unref(input);
        return;
    }

    auto ms = it->second;

    // get filter input name if frame needs filtering
    std::string inputName = ms.name;

    emptyFilterGraph();
    if (filter) {
        filter->feedInput(input, inputName);
    }

    av_frame_free(&input);
}

} // namespace ring
