/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "video_mixer.h"
#include "media_buffer.h"
#include "client/videomanager.h"
#include "manager.h"
#include "media_filter.h"
#include "sinkclient.h"
#include "logger.h"
#include "filter_transpose.h"
#include "sip/sipcall.h"
#ifdef RING_ACCEL
#include "accel.h"
#endif
#include "connectivity/sip_utils.h"
#include "string_utils.h"

#include <cmath>
#include <unistd.h>
#include <mutex>

#include "videomanager_interface.h"
#include <opendht/thread_pool.h>

static constexpr auto MIN_LINE_ZOOM
    = 6; // Used by the ONE_BIG_WITH_SMALL layout for the small previews

namespace jami {
namespace video {

struct VideoMixer::VideoMixerSource
{
    Observable<std::shared_ptr<MediaFrame>>* source {nullptr};
    int rotation {0};
    std::unique_ptr<MediaFilter> rotationFilter {nullptr};
    std::shared_ptr<VideoFrame> render_frame;
    void atomic_copy(const VideoFrame& other)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto newFrame = std::make_shared<VideoFrame>();
        newFrame->copyFrom(other);
        render_frame = newFrame;
    }

    std::shared_ptr<VideoFrame> getRenderFrame()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return render_frame;
    }

    // Current render informations
    int x {};
    int y {};
    int w {};
    int h {};
    bool hasVideo {true};

private:
    std::mutex mutex_;
};

static constexpr const auto MIXER_FRAMERATE = 30;
static constexpr const auto FRAME_DURATION = std::chrono::duration<double>(1. / MIXER_FRAMERATE);

VideoMixer::VideoMixer(const std::string& id, const std::string& localInput, bool attachHost)
    : VideoGenerator::VideoGenerator()
    , CallStreamsManager::CallStreamsManager()
    , id_(id)
    , sink_(Manager::instance().createSinkClient(id, true))
    , loop_([] { return true; }, std::bind(&VideoMixer::process, this), [] {})
{
    // Local video camera is the main participant
    if (not localInput.empty() && attachHost) {
        auto videoInput = getVideoInput(localInput);
        localInputs_.emplace_back(videoInput);
        attachVideo(videoInput.get(),
                    "",
                    sip_utils::streamId("", sip_utils::DEFAULT_VIDEO_STREAMID));
    }
    loop_.start();
    nextProcess_ = std::chrono::steady_clock::now();

    JAMI_DBG("[mixer:%s] New instance created", id_.c_str());


    auto conf_res = split_string_to_unsigned(jami::Manager::instance()
                                                 .videoPreferences.getConferenceResolution(),
                                             'x');
    if (conf_res.size() == 2u) {
#if defined(__APPLE__) && TARGET_OS_MAC
        setParameters(conf_res[0], conf_res[1], AV_PIX_FMT_NV12);
#else
        setParameters(conf_res[0], conf_res[1]);
#endif
    } else {
        JAMI_ERR("Conference resolution is invalid");
    }
}

VideoMixer::~VideoMixer()
{
    stopSink();
    stopInputs();

    loop_.join();

    JAMI_DBG("[mixer:%s] Instance destroyed", id_.c_str());
}

void
VideoMixer::switchInputs(const std::vector<std::string>& inputs)
{
    // Do not stop video inputs that are already there
    // But only detach it to get new index
    std::lock_guard<std::mutex> lk(localInputsMtx_);
    decltype(localInputs_) newInputs;
    for (auto i = 0u; i != inputs.size(); ++i) {
        auto videoInput = getVideoInput(inputs[i]);
        // Note, video can be a previously stopped device (eg. restart a screen sharing)
        // in this case, the videoInput will be found and must be restarted
        videoInput->restart();
        auto onlyDetach = false;
        auto it = std::find(localInputs_.cbegin(), localInputs_.cend(), videoInput);
        onlyDetach = it != localInputs_.cend();
        newInputs.emplace_back(videoInput);
        if (onlyDetach) {
            videoInput->detach(this);
            localInputs_.erase(it);
        }
    }
    // Stop other video inputs
    stopInputs();
    localInputs_ = std::move(newInputs);

    // Re-attach videoInput to mixer
    for (auto i = 0u; i != localInputs_.size(); ++i)
        attachVideo(localInputs_[i].get(), "", sip_utils::streamId("", fmt::format("video_{}", i)));
}



void
VideoMixer::setStreams(const std::string& uri, const std::string& device, const std::vector<MediaAttribute>& streams)
{
    JAMI_ERROR("@@@ setStreams");
    auto key = std::make_pair(uri, device); // TODO if uri/device is self, switch input
    auto& callInfo = callInfo_[key];
    callInfo.streams.clear();

    // Do not stop video inputs that are already there
    // But only detach it to get new index
    std::lock_guard<std::mutex> lk(localInputsMtx_);
    decltype(localInputs_) newInputs;


    for (const auto& stream : streams) {
        auto streamId = sip_utils::streamId("", stream.label_);
        if (streamId.find("video") != std::string::npos) {
            // Video stream, we can replace audio
            replaceAudioStream(uri, device, streamId);
            auto& sInfo = callInfo.streams[streamId];
            sInfo.videoMuted = stream.muted_;

            auto videoInput = getVideoInput(stream.sourceUri_);
            // Note, video can be a previously stopped device (eg. restart a screen sharing)
            // in this case, the videoInput will be found and must be restarted
            videoInput->restart();
            auto onlyDetach = false;
            auto it = std::find(localInputs_.cbegin(), localInputs_.cend(), videoInput);
            onlyDetach = it != localInputs_.cend();
            newInputs.emplace_back(videoInput);
            if (onlyDetach) {
                videoInput->detach(this);
                localInputs_.erase(it);
            }
        } else {
            // Audio stream, nothing to do if video
            std::string videoStream = streamId;
            string_replace(videoStream, "audio", "video"); // TODO bestStreamId
            if (callInfo.streams.find(videoStream) == callInfo.streams.end()) {
                auto& sInfo = callInfo.streams[streamId];
                sInfo.audioLocalMuted = stream.muted_;
            }
        }
    }
    updateInfo();


    // Stop other video inputs
    stopInputs();
    localInputs_ = std::move(newInputs);

    // Re-attach videoInput to mixer
    // TODO use ID from streams()
    for (auto i = 0u; i != localInputs_.size(); ++i)
        attachVideo(localInputs_[i].get(), "", sip_utils::streamId("", fmt::format("video_{}", i)));
}

void
VideoMixer::stopInput(const std::shared_ptr<VideoFrameActiveWriter>& input)
{
    // Detach videoInputs from mixer
    input->detach(this);
#if !VIDEO_CLIENT_INPUT
    // Stop old VideoInput
    if (auto oldInput = std::dynamic_pointer_cast<VideoInput>(input))
        oldInput->stopInput();
#endif
}

void
VideoMixer::stopInputs()
{
    for (auto& input : localInputs_)
        stopInput(input);
    localInputs_.clear();
}

void
VideoMixer::setLayout(int layout)
{
    JAMI_ERROR("@@@ setLayout");
    if (layout < 0 || layout > 2) {
        JAMI_ERROR("Unknown layout {}", layout);
        return;
    }
    currentLayout_ = static_cast<Layout>(layout);
    if (currentLayout_ == Layout::GRID) {
        if (!activeStream_.second.empty())
            callInfo_[activeStream_.first].streams[activeStream_.second].active = false;
        activeStream_ = {};
    }
    layoutUpdated_ += 1;
}

void
VideoMixer::attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame,
                        const std::string& callId,
                        const std::string& streamId)
{
    if (!frame)
        return;
    JAMI_DEBUG("Attaching video with streamId {:s}", streamId);
    if (auto call = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(callId)))
        replaceAudioStream(call->getRemoteUri(), call->getRemoteDeviceId(), streamId);
    {
        std::lock_guard<std::mutex> lk(videoToStreamInfoMtx_);
        videoToStreamInfo_[frame] = StreamInfo {callId, streamId};
        // TODO homogeneize remove
    }
    frame->attach(this);
}

void
VideoMixer::detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame)
{
    if (!frame)
        return;
    bool detach = false;
    std::unique_lock<std::mutex> lk(videoToStreamInfoMtx_);
    auto it = videoToStreamInfo_.find(frame);
    if (it != videoToStreamInfo_.end()) {
        JAMI_DEBUG("Detaching video of call {:s}", it->second.callId);
        detach = true;
        // Handle the case where the current shown source leave the conference
        if (auto call = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(it->second.callId))) {
            auto& si = callInfo_[std::make_pair(call->getRemoteUri(), call->getRemoteDeviceId())];
            si.streams.erase(it->second.streamId);
        }
        videoToStreamInfo_.erase(it);
    }
    lk.unlock();
    if (detach)
        frame->detach(this);
}

void
VideoMixer::attached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    std::unique_lock lock(rwMutex_);

    auto src = std::unique_ptr<VideoMixerSource>(new VideoMixerSource);
    src->render_frame = std::make_shared<VideoFrame>();
    src->source = ob;
    JAMI_DBG("Add new source [%p]", src.get());
    sources_.emplace_back(std::move(src));
    JAMI_DEBUG("Total sources: {:d}", sources_.size());
    JAMI_ERROR("@@@ ATTACHED");
    layoutUpdated_ += 1;
}

void
VideoMixer::detached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    std::unique_lock lock(rwMutex_);

    for (const auto& x : sources_) {
        if (x->source == ob) {
            JAMI_DBG("Remove source [%p]", x.get());
            sources_.remove(x);
            JAMI_DEBUG("Total sources: {:d}", sources_.size());
            JAMI_ERROR("@@@ DETACHED");
            layoutUpdated_ += 1;
            break;
        }
    }
}

void
VideoMixer::update(Observable<std::shared_ptr<MediaFrame>>* ob,
                   const std::shared_ptr<MediaFrame>& frame_p)
{
    std::shared_lock lock(rwMutex_);

    for (const auto& x : sources_) {
        if (x->source == ob) {
#ifdef RING_ACCEL
            std::shared_ptr<VideoFrame> frame;
            try {
                frame = HardwareAccel::transferToMainMemory(*std::static_pointer_cast<VideoFrame>(
                                                                frame_p),
                                                            AV_PIX_FMT_NV12);
                x->atomic_copy(*std::static_pointer_cast<VideoFrame>(frame));
            } catch (const std::runtime_error& e) {
                JAMI_ERR("[mixer:%s] Accel failure: %s", id_.c_str(), e.what());
                return;
            }
#else
            x->atomic_copy(*std::static_pointer_cast<VideoFrame>(frame_p));
#endif
            return;
        }
    }
}

void
VideoMixer::process()
{
    nextProcess_ += std::chrono::duration_cast<std::chrono::microseconds>(FRAME_DURATION);
    const auto delay = nextProcess_ - std::chrono::steady_clock::now();
    if (delay.count() > 0)
        std::this_thread::sleep_for(delay);

    // Nothing to do.
    if (width_ == 0 or height_ == 0) {
        return;
    }

    VideoFrame& output = getNewFrame();
    try {
        output.reserve(format_, width_, height_);
    } catch (const std::bad_alloc& e) {
        JAMI_ERR("[mixer:%s] VideoFrame::allocBuffer() failed", id_.c_str());
        return;
    }

    libav_utils::fillWithBlack(output.pointer());

    {
        std::lock_guard<std::mutex> lk(audioOnlySourcesMtx_);
        std::shared_lock lock(rwMutex_);

        int i = 0;
        bool activeFound = false;
        bool needsUpdate = layoutUpdated_ > 0;
        bool successfullyRendered = audioOnlySources_.size() != 0 && sources_.size() == 0;
        // add all audioonlysources
        // TODO uses infos to check if videoMuted
        for (auto& [callId, streamId] : audioOnlySources_) {
            if (auto call = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(callId))) {
                auto& si = callInfo_[std::make_pair(call->getRemoteUri(), call->getRemoteDeviceId())].streams[streamId];
                si.x = 0; si.y = 0; si.w = 10; si.h = 10;
                if (currentLayout_ == Layout::ONE_BIG and si.active)
                    successfullyRendered = true;
            }

        }
        // add video sources
        for (auto& x : sources_) {
            /* thread stop pending? */
            if (!loop_.isRunning())
                return;

            auto sinfo = streamInfo(x->source);
            decltype(callInfo_)::iterator ci;
            if (sinfo.callId.empty()) {
                // Host
                ci = callInfo_.find(std::make_pair(uri_, deviceId_));
            } else {
                auto call = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(sinfo.callId));
                if (!call)
                    continue;
                ci = callInfo_.find(std::make_pair(call->getRemoteUri(), call->getRemoteDeviceId()));
            }
            if (ci == callInfo_.end())
                continue;
            auto si = ci->second.streams.find(sinfo.streamId);
            if (si == ci->second.streams.end())
                continue;

            if (currentLayout_ != Layout::ONE_BIG or si->second.active) {
                // make rendered frame temporarily unavailable for update()
                // to avoid concurrent access.
                std::shared_ptr<VideoFrame> input = x->getRenderFrame();
                std::shared_ptr<VideoFrame> fooInput = std::make_shared<VideoFrame>();

                auto wantedIndex = i;
                if (currentLayout_ == Layout::ONE_BIG) {
                    wantedIndex = 0;
                    activeFound = true;
                } else if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL) {
                    if (si->second.active) {
                        wantedIndex = 0;
                        activeFound = true;
                    } else if (not activeFound) {
                        wantedIndex += 1;
                    }
                }

                auto hasVideo = x->hasVideo;
                bool blackFrame = false;

                if (!input->height() or !input->width()) {
                    successfullyRendered = true;
                    fooInput->reserve(format_, width_, height_);
                    blackFrame = true;
                } else {
                    fooInput.swap(input);
                }

                // If orientation changed or if the first valid frame for source
                // is received -> trigger layout calculation and confInfo update
                if (x->rotation != fooInput->getOrientation() or !x->w or !x->h) {
                    layoutUpdated_ += 1;
                    needsUpdate = true;
                }

                if (needsUpdate)
                    calc_position(x, fooInput, wantedIndex);

                if (!blackFrame) {
                    if (fooInput)
                        successfullyRendered |= render_frame(output, fooInput, x);
                    else
                        JAMI_WARN("[mixer:%s] Nothing to render for %p", id_.c_str(), x->source);
                }

                si->second.videoMuted = blackFrame || !successfullyRendered;
                if (hasVideo != x->hasVideo) {
                    layoutUpdated_ += 1;
                    needsUpdate = true;
                }
                si->second.x = x->x;
                si->second.y = x->y;
                si->second.w = x->w;
                si->second.h = x->h;
            } else if (needsUpdate) {
                si->second.x = 0;
                si->second.y = 0;
                si->second.w = 0;
                si->second.h = 0;
                si->second.videoMuted = true;
            }

            ++i;
        }
        if (needsUpdate and successfullyRendered) {
            layoutUpdated_ -= 1;
            if (layoutUpdated_ == 0)
                updateInfo();
        }
    }

    output.pointer()->pts = av_rescale_q_rnd(av_gettime() - startTime_,
                                             {1, AV_TIME_BASE},
                                             {1, MIXER_FRAMERATE},
                                             static_cast<AVRounding>(AV_ROUND_NEAR_INF
                                                                     | AV_ROUND_PASS_MINMAX));
    lastTimestamp_ = output.pointer()->pts;
    publishFrame();
}

bool
VideoMixer::render_frame(VideoFrame& output,
                         const std::shared_ptr<VideoFrame>& input,
                         std::unique_ptr<VideoMixerSource>& source)
{
    if (!width_ or !height_ or !input->pointer() or input->pointer()->format == -1)
        return false;

    int cell_width = source->w;
    int cell_height = source->h;
    int xoff = source->x;
    int yoff = source->y;

    int angle = input->getOrientation();
    const constexpr char filterIn[] = "mixin";
    if (angle != source->rotation) {
        source->rotationFilter = video::getTransposeFilter(angle,
                                                           filterIn,
                                                           input->width(),
                                                           input->height(),
                                                           input->format(),
                                                           false);
        source->rotation = angle;
    }
    std::shared_ptr<VideoFrame> frame;
    if (source->rotationFilter) {
        source->rotationFilter->feedInput(input->pointer(), filterIn);
        frame = std::static_pointer_cast<VideoFrame>(
            std::shared_ptr<MediaFrame>(source->rotationFilter->readOutput()));
    } else {
        frame = input;
    }

    scaler_.scale_and_pad(*frame, output, xoff, yoff, cell_width, cell_height, true);
    return true;
}

void
VideoMixer::calc_position(std::unique_ptr<VideoMixerSource>& source,
                          const std::shared_ptr<VideoFrame>& input,
                          int index)
{
    if (!width_ or !height_)
        return;

    // Compute cell size/position
    int cell_width, cell_height, cellW_off, cellH_off;
    const int n = currentLayout_ == Layout::ONE_BIG ? 1 : sources_.size();
    const int zoom = currentLayout_ == Layout::ONE_BIG_WITH_SMALL ? std::max(MIN_LINE_ZOOM, n)
                                                                  : ceil(sqrt(n));
    if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL && index == 0) {
        // In ONE_BIG_WITH_SMALL, the first line at the top is the previews
        // The rest is the active source
        cell_width = width_;
        cell_height = height_ - height_ / zoom;
    } else {
        cell_width = width_ / zoom;
        cell_height = height_ / zoom;
    }
    if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL) {
        if (index == 0) {
            cellW_off = 0;
            cellH_off = height_ / zoom; // First line height
        } else {
            cellW_off = (index - 1) * cell_width;
            // Show sources in center
            cellW_off += (width_ - (n - 1) * cell_width) / 2;
            cellH_off = 0;
        }
    } else {
        cellW_off = (index % zoom) * cell_width;
        if (currentLayout_ == Layout::GRID && n % zoom != 0 && index >= (zoom * ((n - 1) / zoom))) {
            // Last line, center participants if not full
            cellW_off += (width_ - (n % zoom) * cell_width) / 2;
        }
        cellH_off = (index / zoom) * cell_height;
    }

    // Compute frame size/position
    float zoomW, zoomH;
    int frameW, frameH, frameW_off, frameH_off;

    if (input->getOrientation() % 180) {
        // Rotated frame
        zoomW = (float) input->height() / cell_width;
        zoomH = (float) input->width() / cell_height;
        frameH = std::round(input->width() / std::max(zoomW, zoomH));
        frameW = std::round(input->height() / std::max(zoomW, zoomH));
    } else {
        zoomW = (float) input->width() / cell_width;
        zoomH = (float) input->height() / cell_height;
        frameW = std::round(input->width() / std::max(zoomW, zoomH));
        frameH = std::round(input->height() / std::max(zoomW, zoomH));
    }

    // Center the frame in the cell
    frameW_off = cellW_off + (cell_width - frameW) / 2;
    frameH_off = cellH_off + (cell_height - frameH) / 2;

    // Update source's cache
    source->w = frameW;
    source->h = frameH;
    source->x = frameW_off;
    source->y = frameH_off;
}

void
VideoMixer::setParameters(int width, int height, AVPixelFormat format)
{
    std::unique_lock lock(rwMutex_);

    width_ = width;
    height_ = height;
    format_ = format;

    // cleanup the previous frame to have a nice copy in rendering method
    std::shared_ptr<VideoFrame> previous_p(obtainLastFrame());
    if (previous_p)
        libav_utils::fillWithBlack(previous_p->pointer());

    startSink();
    JAMI_ERROR("@@@ SET PARAMETERS");
    layoutUpdated_ += 1;
    startTime_ = av_gettime();
}

void
VideoMixer::startSink()
{
    stopSink();

    if (width_ == 0 or height_ == 0) {
        JAMI_WARN("[mixer:%s] MX: unable to start with zero-sized output", id_.c_str());
        return;
    }

    if (not sink_->start()) {
        JAMI_ERR("[mixer:%s] MX: sink startup failed", id_.c_str());
        return;
    }

    if (this->attach(sink_.get()))
        sink_->setFrameSize(width_, height_);
}

void
VideoMixer::stopSink()
{
    this->detach(sink_.get());
    sink_->stop();
}

int
VideoMixer::getWidth() const
{
    return width_;
}

int
VideoMixer::getHeight() const
{
    return height_;
}

AVPixelFormat
VideoMixer::getPixelFormat() const
{
    return format_;
}

MediaStream
VideoMixer::getStream(const std::string& name) const
{
    MediaStream ms;
    ms.name = name;
    ms.format = format_;
    ms.isVideo = true;
    ms.height = height_;
    ms.width = width_;
    ms.frameRate = {MIXER_FRAMERATE, 1};
    ms.timeBase = {1, MIXER_FRAMERATE};
    ms.firstTimestamp = lastTimestamp_;

    return ms;
}

} // namespace video
} // namespace jami
