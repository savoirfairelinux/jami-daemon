/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

    ~StreamObserver()
    {
        while (observablesFrames_.size() > 0) {
            auto obs = observablesFrames_.begin();
            (*obs)->detach(this);
            // it should be erased from observablesFrames_ in detach. If it does not happens erase frame to avoid infinite loop.
            auto it = observablesFrames_.find(*obs);
            if (it != observablesFrames_.end())
                observablesFrames_.erase(it);
        }
    };

    void update(Observable<std::shared_ptr<MediaFrame>>* /*ob*/,
                const std::shared_ptr<MediaFrame>& m) override
    {
        if (not isEnabled)
            return;
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

    void attached(Observable<std::shared_ptr<MediaFrame>>* obs) override
    {
        observablesFrames_.insert(obs);
    }

    void detached(Observable<std::shared_ptr<MediaFrame>>* obs) override
    {
        auto it = observablesFrames_.find(obs);
        if (it != observablesFrames_.end())
            observablesFrames_.erase(it);
    }

    bool isEnabled = false;
private:
    std::function<void(const std::shared_ptr<MediaFrame>&)> cb_;
    std::unique_ptr<MediaFilter> videoRotationFilter_ {};
    int rotation_ = 0;
    std::set<Observable<std::shared_ptr<MediaFrame>>*> observablesFrames_;
};

MediaRecorder::MediaRecorder() {}

MediaRecorder::~MediaRecorder()
{
    flush();
    reset();
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
    startTimeStamp_ = av_gettime();

    std::lock_guard lk(encoderMtx_);
    encoder_.reset(new MediaEncoder);

    JAMI_LOG("Start recording '{}'", getPath());
    if (initRecord() >= 0) {
        isRecording_ = true;
        {
            std::lock_guard lk(mutexStreamSetup_);
            for (auto& media : streams_) {
                if (media.second->info.isVideo) {
                    std::lock_guard lk2(mutexFilterVideo_);
                    setupVideoOutput();
                } else {
                    std::lock_guard lk2(mutexFilterAudio_);
                    setupAudioOutput();
                }
                media.second->isEnabled = true;
            }
        }
        // start thread after isRecording_ is set to true
        dht::ThreadPool::computation().run([rec = shared_from_this()] {
            std::lock_guard lk(rec->encoderMtx_);
            while (rec->isRecording()) {
                std::shared_ptr<MediaFrame> frame;
                // get frame from queue
                {
                    std::unique_lock lk(rec->mutexFrameBuff_);
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
                    if (rec->encoder_ && frame && frame->pointer()) {
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
    interrupted_ = false;
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
        {
            std::lock_guard lk(mutexStreamSetup_);
            for (auto& media : streams_) {
                media.second->isEnabled = false;
            }
        }
        emitSignal<libjami::CallSignal::RecordPlaybackStopped>(getPath());
    }
}

Observer<std::shared_ptr<MediaFrame>>*
MediaRecorder::addStream(const MediaStream& ms)
{
    std::lock_guard lk(mutexStreamSetup_);
    if (audioOnly_ && ms.isVideo) {
        JAMI_ERR() << "Attempting to add video stream to audio only recording";
        return nullptr;
    }
    if (ms.format < 0 || ms.name.empty()) {
        JAMI_ERR() << "Attempting to add invalid stream to recording";
        return nullptr;
    }

    auto it = streams_.find(ms.name);
    if (it == streams_.end()) {
        auto streamPtr = std::make_unique<StreamObserver>(ms,
                                                          [this,
                                                           ms](const std::shared_ptr<MediaFrame>& frame) {
                                                              onFrame(ms.name, frame);
                                                          });
        it = streams_.insert(std::make_pair(ms.name, std::move(streamPtr))).first;
        JAMI_LOG("[Recorder: {:p}] Recorder input #{}: {:s}", fmt::ptr(this), streams_.size(), ms.name);
    } else {
        if (ms == it->second->info)
            JAMI_LOG("[Recorder: {:p}] Recorder already has '{:s}' as input", fmt::ptr(this), ms.name);
        else {
            it->second
                = std::make_unique<StreamObserver>(ms,
                                                   [this,
                                                    ms](const std::shared_ptr<MediaFrame>& frame) {
                                                       onFrame(ms.name, frame);
                                                   });
        }
    }
    it->second->isEnabled = isRecording_;
    return it->second.get();
}

void
MediaRecorder::removeStream(const MediaStream& ms)
{
    std::lock_guard lk(mutexStreamSetup_);

    auto it = streams_.find(ms.name);
    if (it == streams_.end()) {
        JAMI_LOG("[Recorder: {:p}] Recorder no stream to remove", fmt::ptr(this));
    } else {
        JAMI_LOG("[Recorder: {:p}] Recorder removing '{:s}'", fmt::ptr(this), ms.name);
        streams_.erase(it);
        if (ms.isVideo)
            setupVideoOutput();
        else
            setupAudioOutput();
    }
    return;
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
    if (not isRecording_ || interrupted_)
        return;

    std::lock_guard lk(mutexStreamSetup_);

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
    std::vector<std::unique_ptr<MediaFrame>> filteredFrames;
#ifdef ENABLE_VIDEO
    if (ms.isVideo && videoFilter_ && outputVideoFilter_) {
        std::lock_guard lk(mutexFilterVideo_);
                videoFilter_->feedInput(clone->pointer(), name);
        auto videoFilterOutput = videoFilter_->readOutput();
        if (videoFilterOutput) {
            outputVideoFilter_->feedInput(videoFilterOutput->pointer(), "input");
            while (auto fFrame = outputVideoFilter_->readOutput()) {
                filteredFrames.emplace_back(std::move(fFrame));
            }
        }
    } else if (audioFilter_ && outputAudioFilter_) {
#endif // ENABLE_VIDEO
        std::lock_guard lkA(mutexFilterAudio_);
        audioFilter_->feedInput(clone->pointer(), name);
        auto audioFilterOutput = audioFilter_->readOutput();
        if (audioFilterOutput) {
            outputAudioFilter_->feedInput(audioFilterOutput->pointer(), "input");
            filteredFrames.emplace_back(outputAudioFilter_->readOutput());
        }
#ifdef ENABLE_VIDEO
    }
#endif // ENABLE_VIDEO

    for (auto& fFrame : filteredFrames) {
        std::lock_guard lk(mutexFrameBuff_);
        frameBuff_.emplace_back(std::move(fFrame));
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
#endif // ENABLE_VIDEO

    {
        MediaStream audioStream;
        audioStream.name = "audioOutput";
        audioStream.format = 1;
        audioStream.timeBase = rational<int>(1, 48000);
        audioStream.sampleRate = 48000;
        audioStream.nbChannels = 2;
        encoder_->setOptions(audioStream);

        auto audioCodec = std::static_pointer_cast<jami::SystemAudioCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("opus", jami::MEDIA_AUDIO));
        audioIdx_ = encoder_->addStream(*audioCodec.get());
        if (audioIdx_ < 0) {
            JAMI_ERR() << "Failed to add audio stream to encoder";
            return -1;
        }
    }

#ifdef ENABLE_VIDEO
    if (!audioOnly_) {
        MediaStream videoStream;

        videoStream.name = "videoOutput";
        videoStream.format = 0;
        videoStream.isVideo = true;
        videoStream.timeBase = rational<int>(0, 1);
        videoStream.width = 1280;
        videoStream.height = 720;
        videoStream.frameRate = rational<int>(30, 1);
        videoStream.bitrate = Manager::instance().videoPreferences.getRecordQuality();

        MediaDescription args;
        args.mode = RateMode::CQ;
        encoder_->setOptions(videoStream);
        encoder_->setOptions(args);

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

void
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
        JAMI_WARN() << "Attempting to record a video stream but none is valid";
        return;
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
            JAMI_ERR("Attempting to record a stream but none is valid");
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
    if (ret < 0) {
        JAMI_ERR() << "Failed to initialize video filter";
    }

    // setup output filter
    if (!videoFilter_)
        return;
    MediaStream secondaryFilter = videoFilter_->getOutputParams();
    secondaryFilter.name = "input";
    if (outputVideoFilter_) {
        outputVideoFilter_->flush();
        outputVideoFilter_.reset();
    }

    outputVideoFilter_.reset(new MediaFilter);

    float scaledHeight = 1280 * (float)secondaryFilter.height / (float)secondaryFilter.width;
    std::string scaleFilter = "scale=1280:-2";
    if (scaledHeight > 720)
        scaleFilter += ",scale=-2:720";

    std::ostringstream f;
    f << "[input]" << scaleFilter
      << ",pad=1280:720:(ow-iw)/2:(oh-ih)/2,format=pix_fmts=yuv420p,fps=30";

    ret = outputVideoFilter_->initialize(f.str(), {secondaryFilter});

    if (ret < 0) {
        JAMI_ERR() << "Failed to initialize output video filter";
    }

#endif

    return;
}

std::string
MediaRecorder::buildVideoFilter(const std::vector<MediaStream>& peers,
                                const MediaStream& local) const
{
    std::ostringstream v;

    switch (peers.size()) {
    case 0:
        v << "[" << local.name << "] fps=30, format=pix_fmts=yuv420p";
        break;
    case 1: {
        auto p = peers[0];
        const constexpr int minHeight = 720;
        const bool needScale = (p.height < minHeight);
        const int newHeight = (needScale ? minHeight : p.height);

        // NOTE -2 means preserve aspect ratio and have the new number be even
        if (needScale)
            v << "[" << p.name << "] fps=30, scale=-2:" << newHeight
              << " [v:m]; ";
        else
            v << "[" << p.name << "] fps=30 [v:m]; ";

        v << "[" << local.name << "] fps=30, scale=-2:" << newHeight / 5
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

void
MediaRecorder::setupAudioOutput()
{
    MediaStream encoderStream;

    // resample to common audio format, so any player can play the file
    audioFilter_.reset(new MediaFilter);
    int ret = -1;

    if (streams_.empty()) {
        JAMI_WARN() << "Attempting to record a audio stream but none is valid";
        return;
    }

    std::vector<MediaStream> peers {};
    for (const auto& media : streams_) {
        if (!media.second->info.isVideo && media.second->info.isValid())
            peers.emplace_back(media.second->info);
    }

    ret = audioFilter_->initialize(buildAudioFilter(peers), peers);

    if (ret < 0) {
        JAMI_ERR() << "Failed to initialize audio filter";
        return;
    }

    // setup output filter
    if (!audioFilter_)
        return;
    MediaStream secondaryFilter = audioFilter_->getOutputParams();
    secondaryFilter.name = "input";
    if (outputAudioFilter_) {
        outputAudioFilter_->flush();
        outputAudioFilter_.reset();
    }

    outputAudioFilter_.reset(new MediaFilter);
    ret = outputAudioFilter_->initialize(
        "[input]aformat=sample_fmts=s16:sample_rates=48000:channel_layouts=stereo",
        {secondaryFilter});

    if (ret < 0) {
        JAMI_ERR() << "Failed to initialize output audio filter";
    }

    return;
}

std::string
MediaRecorder::buildAudioFilter(const std::vector<MediaStream>& peers) const
{
    std::string baseFilter = "aresample=osr=48000:ochl=stereo:osf=s16";
    std::ostringstream a;

    for (const auto& ms : peers)
        a << "[" << ms.name << "] ";
    a << " amix=inputs=" << peers.size() << ", " << baseFilter;
    return a.str();
}

void
MediaRecorder::flush()
{
    {
        std::lock_guard lk(mutexFilterVideo_);
        if (videoFilter_)
            videoFilter_->flush();
        if (outputVideoFilter_)
            outputVideoFilter_->flush();
    }
    {
        std::lock_guard lk(mutexFilterAudio_);
        if (audioFilter_)
            audioFilter_->flush();
        if (outputAudioFilter_)
            outputAudioFilter_->flush();
    }
    if (encoder_)
        encoder_->flush();
}

void
MediaRecorder::reset()
{
    {
        std::lock_guard lk(mutexFrameBuff_);
        frameBuff_.clear();
    }
    videoIdx_ = audioIdx_ = -1;
    {
        std::lock_guard lk(mutexStreamSetup_);
        {
            std::lock_guard lk2(mutexFilterVideo_);
            videoFilter_.reset();
            outputVideoFilter_.reset();
        }
        {
            std::lock_guard lk2(mutexFilterAudio_);
            audioFilter_.reset();
            outputAudioFilter_.reset();
        }
    }
    encoder_.reset();
}

} // namespace jami
