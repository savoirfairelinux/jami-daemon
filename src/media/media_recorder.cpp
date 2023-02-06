/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include "manager.h"
#include "media_io_handle.h"
#include "media_recorder.h"
#include "system_codec_container.h"
#include "video/filter_transpose.h"
#ifdef ENABLE_VIDEO
#ifdef RING_ACCEL
#include "video/accel.h"
#endif
#endif

#include <opendht/thread_pool.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <sys/types.h>
#include <ctime>

namespace jami {

const constexpr char ROTATION_FILTER_INPUT_NAME[] = "in";

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

struct MediaRecorder::StreamObserver : public Observer<std::shared_ptr<MediaFrame>>
{
    const MediaStream info;

    StreamObserver(const MediaStream& ms,
                   std::function<void(const std::shared_ptr<MediaFrame>&)> func)
        : info(ms)
        , cb_(func) {};

    ~StreamObserver() {};

    void update(Observable<std::shared_ptr<MediaFrame>>* /*ob*/,
                const std::shared_ptr<MediaFrame>& m) override
    {
#ifdef ENABLE_VIDEO
        if (info.isVideo) {
            std::shared_ptr<VideoFrame> framePtr;
#ifdef RING_ACCEL
            auto desc = av_pix_fmt_desc_get(
                (AVPixelFormat) (std::static_pointer_cast<VideoFrame>(m))->format());
            if (desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
                try {
                    framePtr = jami::video::HardwareAccel::transferToMainMemory(
                        *std::static_pointer_cast<VideoFrame>(m), AV_PIX_FMT_NV12);
                } catch (const std::runtime_error& e) {
                    JAMI_ERR("Accel failure: %s", e.what());
                    return;
                }
            } else
#endif
                framePtr = std::static_pointer_cast<VideoFrame>(m);
            int angle = framePtr->getOrientation();
            if (angle != rotation_) {
                videoRotationFilter_ = jami::video::getTransposeFilter(angle,
                                                                       ROTATION_FILTER_INPUT_NAME,
                                                                       framePtr->width(),
                                                                       framePtr->height(),
                                                                       framePtr->format(),
                                                                       true);
                rotation_ = angle;
            }
            if (videoRotationFilter_) {
                videoRotationFilter_->feedInput(framePtr->pointer(), ROTATION_FILTER_INPUT_NAME);
                auto rotated = videoRotationFilter_->readOutput();
                av_frame_remove_side_data(rotated->pointer(), AV_FRAME_DATA_DISPLAYMATRIX);
                cb_(std::move(rotated));
            } else {
                cb_(m);
            }
        } else {
#endif
            cb_(m);
#ifdef ENABLE_VIDEO
        }
#endif
    }

private:
    std::function<void(const std::shared_ptr<MediaFrame>&)> cb_;
    std::unique_ptr<MediaFilter> videoRotationFilter_ {};
    int rotation_ = 0;
};

MediaRecorder::MediaRecorder() {}

MediaRecorder::~MediaRecorder() {}

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
    startTimeStamp_ = av_gettime();

    encoder_.reset(new MediaEncoder);

    JAMI_DBG() << "Start recording '" << getPath() << "'";
    if (initRecord() >= 0) {
        isRecording_ = true;
        // start thread after isRecording_ is set to true
        dht::ThreadPool::computation().run([rec = shared_from_this()] {
            while (rec->isRecording()) {
                std::shared_ptr<MediaFrame> frame;
                // get frame from queue
                {
                    std::unique_lock<std::mutex> lk(rec->mutexFrameBuff_);
                    rec->cv_.wait(lk, [rec] {
                        return rec->interrupted_ or not rec->frameBuff_.empty();
                    });
                    if (rec->interrupted_) {
                        break;
                    }
                    frame = std::move(rec->frameBuff_.front());
                    rec->frameBuff_.pop_front();
                }
                try {
                    // encode frame
                    if (frame && frame->pointer()) {
#ifdef ENABLE_VIDEO
                        bool isVideo = (frame->pointer()->width > 0 && frame->pointer()->height > 0);
                        rec->encoder_->encode(frame->pointer(),
                                              isVideo ? rec->videoIdx_ : rec->audioIdx_);
#else
                        rec->encoder_->encode(frame->pointer(), rec->audioIdx_);
#endif // ENABLE_VIDEO
                    }
                } catch (const MediaEncoderException& e) {
                    JAMI_ERR() << "Failed to record frame: " << e.what();
                }
            }
            rec->flush();
            rec->reset(); // allows recorder to be reused in same call
        });
    }
    return 0;
}

void
MediaRecorder::stopRecording()
{
    interrupted_ = true;
    cv_.notify_all();
    if (isRecording_) {
        JAMI_DBG() << "Stop recording '" << getPath() << "'";
        isRecording_ = false;
        emitSignal<libjami::CallSignal::RecordPlaybackStopped>(getPath());
    }
}

Observer<std::shared_ptr<MediaFrame>>*
MediaRecorder::addStream(const MediaStream& ms)
{
    if (audioOnly_ && ms.isVideo) {
        JAMI_ERR() << "Trying to add video stream to audio only recording";
        return nullptr;
    }
    if (ms.isVideo && ms.format < 0) {
        JAMI_ERR() << "Trying to add invalid video stream to recording";
        return nullptr;
    }

    auto ptr = std::make_unique<StreamObserver>(ms,
                                                [this,
                                                 ms](const std::shared_ptr<MediaFrame>& frame) {
                                                    onFrame(ms.name, frame);
                                                });
    auto p = streams_.insert(std::make_pair(ms.name, std::move(ptr)));
    if (p.second) {
        JAMI_DBG() << "Recorder input #" << streams_.size() << ": " << ms;
        if (ms.isVideo)
            hasVideo_ = true;
        else
            hasAudio_ = true;
        return p.first->second.get();
    } else {
        JAMI_WARN() << "Recorder already has '" << ms.name << "' as input";
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
    if (not isRecording_)
        return;

    // copy frame to not mess with the original frame's pts (does not actually copy frame data)
    std::unique_ptr<MediaFrame> clone;
    const auto& ms = streams_[name]->info;
#if defined(ENABLE_VIDEO) && defined(RING_ACCEL)
    if (ms.isVideo) {
        auto desc = av_pix_fmt_desc_get(
            (AVPixelFormat) (std::static_pointer_cast<VideoFrame>(frame))->format());
        if (desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            try {
                clone = video::HardwareAccel::transferToMainMemory(
                    *std::static_pointer_cast<VideoFrame>(frame),
                    static_cast<AVPixelFormat>(ms.format));
            } catch (const std::runtime_error& e) {
                JAMI_ERR("Accel failure: %s", e.what());
                return;
            }
        } else {
            clone = std::make_unique<MediaFrame>();
            clone->copyFrom(*frame);
        }
    } else {
#endif // ENABLE_VIDEO && RING_ACCEL
        clone = std::make_unique<MediaFrame>();
        clone->copyFrom(*frame);
#if defined(ENABLE_VIDEO) && defined(RING_ACCEL)
    }
#endif // ENABLE_VIDEO && RING_ACCEL
    clone->pointer()->pts = av_rescale_q_rnd(av_gettime() - startTimeStamp_,
                                             {1, AV_TIME_BASE},
                                             ms.timeBase,
                                             static_cast<AVRounding>(AV_ROUND_NEAR_INF
                                                                     | AV_ROUND_PASS_MINMAX));
    std::unique_ptr<MediaFrame> filteredFrame;
#ifdef ENABLE_VIDEO
    if (ms.isVideo) {
        std::lock_guard<std::mutex> lk(mutexFilterVideo_);
        videoFilter_->feedInput(clone->pointer(), name);
        filteredFrame = videoFilter_->readOutput();
    } else {
#endif // ENABLE_VIDEO
        std::lock_guard<std::mutex> lk(mutexFilterAudio_);
        audioFilter_->feedInput(clone->pointer(), name);
        filteredFrame = audioFilter_->readOutput();
#ifdef ENABLE_VIDEO
    }
#endif // ENABLE_VIDEO

    if (filteredFrame) {
        std::lock_guard<std::mutex> lk(mutexFrameBuff_);
        frameBuff_.emplace_back(std::move(filteredFrame));
        cv_.notify_one();
    }
}

int
MediaRecorder::initRecord()
{
    // need to get encoder parameters before calling openFileOutput
    // openFileOutput needs to be called before adding any streams

    std::stringstream timestampString;
    timestampString << std::put_time(&startTime_, "%Y-%m-%d %H:%M:%S");

    if (title_.empty()) {
        title_ = "Conversation at %TIMESTAMP";
    }
    title_ = replaceAll(title_, "%TIMESTAMP", timestampString.str());

    if (description_.empty()) {
        description_ = "Recorded with Jami https://jami.net";
    }
    description_ = replaceAll(description_, "%TIMESTAMP", timestampString.str());

    encoder_->setMetadata(title_, description_);
    encoder_->openOutput(getPath());
#ifdef ENABLE_VIDEO
#ifdef RING_ACCEL
    encoder_->enableAccel(false); // TODO recorder has problems with hardware encoding
#endif

    videoFilter_.reset();
    if (hasVideo_) {
        const MediaStream& videoStream = setupVideoOutput();
        if (videoStream.format < 0) {
            JAMI_ERR() << "Could not retrieve video recorder stream properties";
            return -1;
        }
        MediaDescription args;
        args.mode = RateMode::CQ;
        encoder_->setOptions(videoStream);
        encoder_->setOptions(args);
    }
#endif // ENABLE_VIDEO

    audioFilter_.reset();
    if (hasAudio_) {
        const MediaStream& audioStream = setupAudioOutput();
        if (audioStream.format < 0) {
            JAMI_ERR() << "Could not retrieve audio recorder stream properties";
            return -1;
        }
        encoder_->setOptions(audioStream);
    }

    if (hasAudio_) {
        auto audioCodec = std::static_pointer_cast<jami::SystemAudioCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("opus", jami::MEDIA_AUDIO));
        audioIdx_ = encoder_->addStream(*audioCodec.get());
        if (audioIdx_ < 0) {
            JAMI_ERR() << "Failed to add audio stream to encoder";
            return -1;
        }
    }

#ifdef ENABLE_VIDEO
    if (hasVideo_) {
        auto videoCodec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("VP8", jami::MEDIA_VIDEO));
        videoIdx_ = encoder_->addStream(*videoCodec.get());
        if (videoIdx_ < 0) {
            JAMI_ERR() << "Failed to add video stream to encoder";
            return -1;
        }
    }
#endif // ENABLE_VIDEO

    encoder_->setIOContext(nullptr);

    JAMI_DBG() << "Recording initialized";
    return 0;
}

MediaStream
MediaRecorder::setupVideoOutput()
{
    MediaStream encoderStream, peer, local, mixer;
    auto it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair) {
        return pair.second->info.isVideo
               && pair.second->info.name.find("remote") != std::string::npos;
    });
    if (it != streams_.end())
        peer = it->second->info;

    it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair) {
        return pair.second->info.isVideo
               && pair.second->info.name.find("local") != std::string::npos;
    });
    if (it != streams_.end())
        local = it->second->info;

    it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair) {
        return pair.second->info.isVideo
               && pair.second->info.name.find("mixer") != std::string::npos;
    });
    if (it != streams_.end())
        mixer = it->second->info;

    // vp8 supports only yuv420p
    videoFilter_.reset(new MediaFilter);
    int ret = -1;
    int streams = peer.isValid() + local.isValid() + mixer.isValid();
    switch (streams) {
    case 0: {
        JAMI_ERR("Trying to record a stream but none is valid");
        break;
    }
    case 1: {
        MediaStream inputStream;
        if (peer.isValid())
            inputStream = peer;
        else if (local.isValid())
            inputStream = local;
        else if (mixer.isValid())
            inputStream = mixer;
        else {
            JAMI_ERR("Trying to record a stream but none is valid");
            break;
        }

        ret = videoFilter_->initialize(buildVideoFilter({}, inputStream), {inputStream});
        break;
    }
    case 2: // overlay local video over peer video
        ret = videoFilter_->initialize(buildVideoFilter({peer}, local), {peer, local});
        break;
    default:
        JAMI_ERR() << "Recording more than 2 video streams is not supported";
        break;
    }

#ifdef ENABLE_VIDEO
    if (ret >= 0) {
        encoderStream = videoFilter_->getOutputParams();
        encoderStream.bitrate = Manager::instance().videoPreferences.getRecordQuality();
        JAMI_DBG() << "Recorder output: " << encoderStream;
    } else {
        JAMI_ERR() << "Failed to initialize video filter";
    }
#endif

    return encoderStream;
}

std::string
MediaRecorder::buildVideoFilter(const std::vector<MediaStream>& peers,
                                const MediaStream& local) const
{
    std::stringstream v;

    switch (peers.size()) {
    case 0:
        v << "[" << local.name << "] fps=30, format=pix_fmts=yuv420p";
        break;
    case 1: {
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

        v << "[" << local.name << "] fps=" << newFps << ", scale=-2:" << newHeight / 5
          << " [v:o]; ";

        v << "[v:m] [v:o] overlay=main_w-overlay_w:main_h-overlay_h"
          << ", format=pix_fmts=yuv420p";
    } break;
    default:
        JAMI_ERR() << "Video recordings with more than 2 video streams are not supported";
        break;
    }

    return v.str();
}

MediaStream
MediaRecorder::setupAudioOutput()
{
    MediaStream encoderStream, peer, local, mixer;
    auto it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair) {
        return !pair.second->info.isVideo
               && pair.second->info.name.find("remote") != std::string::npos;
    });
    if (it != streams_.end())
        peer = it->second->info;

    it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair) {
        return !pair.second->info.isVideo
               && pair.second->info.name.find("local") != std::string::npos;
    });
    if (it != streams_.end())
        local = it->second->info;

    it = std::find_if(streams_.begin(), streams_.end(), [](const auto& pair) {
        return !pair.second->info.isVideo
               && pair.second->info.name.find("mixer") != std::string::npos;
    });
    if (it != streams_.end())
        local = it->second->info;

    // resample to common audio format, so any player can play the file
    audioFilter_.reset(new MediaFilter);
    int ret = -1;
    int streams = peer.isValid() + local.isValid() + mixer.isValid();
    switch (streams) {
    case 1: {
        MediaStream inputStream;
        if (peer.isValid())
            inputStream = peer;
        else if (local.isValid())
            inputStream = local;
        else if (mixer.isValid())
            inputStream = mixer;
        else {
            JAMI_ERR("Trying to record a stream but none is valid");
            break;
        }
        ret = audioFilter_->initialize(buildAudioFilter({}, inputStream), {inputStream});
        break;
    }
    case 2: // mix both audio streams
        ret = audioFilter_->initialize(buildAudioFilter({peer}, local), {peer, local});
        break;
    default:
        JAMI_ERR() << "Recording more than 2 audio streams is not supported";
        break;
    }

    if (ret >= 0) {
        encoderStream = audioFilter_->getOutputParams();
        JAMI_DBG() << "Recorder output: " << encoderStream;
    } else {
        JAMI_ERR() << "Failed to initialize audio filter";
    }

    return encoderStream;
}

std::string
MediaRecorder::buildAudioFilter(const std::vector<MediaStream>& peers,
                                const MediaStream& local) const
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
    if (videoFilter_) {
        std::lock_guard<std::mutex> lk(mutexFilterVideo_);
        videoFilter_->flush();
    }

    if (audioFilter_) {
        std::lock_guard<std::mutex> lk(mutexFilterAudio_);
        audioFilter_->flush();
    }
    encoder_->flush();
}

void
MediaRecorder::reset()
{
    {
        std::lock_guard<std::mutex> lk(mutexFrameBuff_);
        frameBuff_.clear();
    }
    streams_.clear();
    videoIdx_ = audioIdx_ = -1;
    audioOnly_ = false;
    videoFilter_.reset();
    audioFilter_.reset();
    encoder_.reset();
}

} // namespace jami
