/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
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
#ifdef ENABLE_HWACCEL
#include "accel.h"
#endif
#include "connectivity/sip_utils.h"

#include <cmath>
#include <algorithm>
#include <unistd.h>
#include <mutex>

#include <opendht/thread_pool.h>

static constexpr auto MIN_LINE_ZOOM = 6; // Used by the ONE_BIG_WITH_SMALL layout for the small previews

namespace jami {
namespace video {

struct VideoMixer::VideoMixerSource
{
    Observable<std::shared_ptr<MediaFrame>>* source {nullptr};
    int rotation {0};
    std::unique_ptr<MediaFilter> rotationFilter {nullptr};
    std::shared_ptr<VideoFrame> render_frame;
    // Frames received since the last dynamic-format check, to measure the
    // source frame rate.
    std::atomic<unsigned> framesSinceCheck {0};
    int measuredFps {0};
    void atomic_copy(const VideoFrame& other)
    {
        std::lock_guard lock(mutex_);
        auto newFrame = std::make_shared<VideoFrame>();
        newFrame->copyFrom(other);
        render_frame = newFrame;
        framesSinceCheck.fetch_add(1, std::memory_order_relaxed);
    }

    std::shared_ptr<VideoFrame> getRenderFrame()
    {
        std::lock_guard lock(mutex_);
        return render_frame;
    }

    // Current render information
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

// Dynamic-format policy constants.
static constexpr int MAX_CELL_WIDTH = 1280; // A composed grid cell never needs more than 720p
static constexpr int MAX_CELL_HEIGHT = 720;
static constexpr int MIN_SURFACE_WIDTH = 640;
static constexpr int MIN_SURFACE_HEIGHT = 360;
static constexpr size_t MAX_BIG_CAP_SOURCES = 3; // bigCap allowed only for small conferences
static constexpr auto FORMAT_CHECK_PERIOD = std::chrono::seconds(2);
static constexpr auto MIN_FORMAT_CHANGE_INTERVAL = std::chrono::seconds(5);
static constexpr double GROW_AREA_RATIO = 1.25;
static constexpr double SHRINK_AREA_RATIO = 0.8;

static int
alignDown16(int v)
{
    return v & ~15;
}

static std::pair<int, int>
clampSurface(int w, int h, std::pair<int, int> cap)
{
    if (w > cap.first || h > cap.second) {
        // Fit in the cap, keeping the aspect ratio.
        const double ratio = std::min(static_cast<double>(cap.first) / w, static_cast<double>(cap.second) / h);
        w = static_cast<int>(w * ratio);
        h = static_cast<int>(h * ratio);
    }
    w = std::max(alignDown16(w), MIN_SURFACE_WIDTH);
    h = std::max(alignDown16(h), MIN_SURFACE_HEIGHT);
    return {w, h};
}

static const VideoMixer::SourceSpec*
findActiveSource(const std::vector<VideoMixer::SourceSpec>& sources)
{
    for (const auto& s : sources)
        if (s.active && s.width > 0 && s.height > 0)
            return &s;
    return nullptr;
}

// Largest cell any (optionally inactive-only) source can fill, up to 720p.
static std::pair<int, int>
largestCell(const std::vector<VideoMixer::SourceSpec>& sources, bool inactiveOnly)
{
    int cellW = 0, cellH = 0;
    for (const auto& s : sources) {
        if (inactiveOnly && s.active)
            continue;
        cellW = std::max(cellW, std::min(s.width, MAX_CELL_WIDTH));
        cellH = std::max(cellH, std::min(s.height, MAX_CELL_HEIGHT));
    }
    return {cellW, cellH};
}

// In ONE_BIG_WITH_SMALL the top preview line stays usable: each preview only
// gets a 1/zoom-th of the surface (see calc_position), so a promoted
// low-definition source must not collapse the whole composite below what the
// previews can fill. Previews alone never push the surface past the base cap.
static std::pair<int, int>
growForPreviews(std::pair<int, int> target,
                const std::vector<VideoMixer::SourceSpec>& sources,
                std::pair<int, int> baseCap)
{
    const int zoom = std::max(MIN_LINE_ZOOM, static_cast<int>(sources.size()));
    const auto [cellW, cellH] = largestCell(sources, true);
    if (cellW > 0 && cellH > 0) {
        const auto previews = clampSurface(zoom * cellW, zoom * cellH, baseCap);
        target.first = std::max(target.first, previews.first);
        target.second = std::max(target.second, previews.second);
    }
    return target;
}

VideoMixer::VideoMixer(const std::string& id, const std::string& localInput, bool attachHost)
    : VideoGenerator::VideoGenerator()
    , id_(id)
    , sink_(Manager::instance().createSinkClient(id, true))
    , frameRate_(MIXER_FRAMERATE)
    , loop_([] { return true; }, std::bind(&VideoMixer::process, this), [] {})
{
    // Participant frames are mostly downscaled into their cell: area averaging
    // gives a much cleaner result than the default fast bilinear.
    scaler_.setScalingAlgorithm(SWS_AREA);
    // Local video camera is the main participant
    if (not localInput.empty() && attachHost) {
        auto videoInput = getVideoInput(localInput);
        localInputs_.emplace_back(videoInput);
        attachVideo(videoInput.get(), "", sip_utils::streamId("", sip_utils::DEFAULT_VIDEO_STREAMID));
    }
    loop_.start();
    nextProcess_ = std::chrono::steady_clock::now();

    JAMI_LOG("[mixer:{}] New instance created", id_);
}

VideoMixer::~VideoMixer()
{
    stopSink();
    stopInputs();

    loop_.join();

    JAMI_LOG("[mixer:{}] Instance destroyed", id_);
}

void
VideoMixer::switchInputs(const std::vector<std::string>& inputs)
{
    // Do not stop video inputs that are already there
    // But only detach it to get new index
    std::lock_guard lk(localInputsMtx_);
    decltype(localInputs_) newInputs;
    newInputs.reserve(inputs.size());
    for (const auto& input : inputs) {
        auto videoInput = getVideoInput(input);
        // Note, video can be a previously stopped device (eg. restart a screen sharing)
        // in this case, the videoInput will be found and must be restarted
        videoInput->restart();
        auto it = std::find(localInputs_.cbegin(), localInputs_.cend(), videoInput);
        auto onlyDetach = it != localInputs_.cend();
        if (onlyDetach) {
            videoInput->detach(this);
            localInputs_.erase(it);
        }
        newInputs.emplace_back(std::move(videoInput));
    }
    // Stop other video inputs
    stopInputs();
    localInputs_ = std::move(newInputs);

    // Re-attach videoInput to mixer
    for (size_t i = 0; i < localInputs_.size(); ++i) {
        auto& input = localInputs_[i];
        attachVideo(input.get(), "", sip_utils::streamId("", fmt::format("video_{}", i)));
    }
}

void
VideoMixer::stopInput(const std::shared_ptr<VideoFrameActiveWriter>& input)
{
    // Detach videoInputs from mixer
    input->detach(this);
}

void
VideoMixer::stopInputs()
{
    for (auto& input : localInputs_)
        stopInput(input);
    localInputs_.clear();
}

void
VideoMixer::setActiveStream(const std::string& id)
{
    activeStream_ = id;
    updateLayout();
}

void
VideoMixer::updateLayout()
{
    if (activeStream_ == "")
        currentLayout_ = Layout::GRID;
    layoutUpdated_ += 1;
}

void
VideoMixer::attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame,
                        const std::string& callId,
                        const std::string& streamId)
{
    if (!frame)
        return;
    JAMI_LOG("Attaching video with streamId {}", streamId);
    {
        std::lock_guard lk(videoToStreamInfoMtx_);
        videoToStreamInfo_[frame] = StreamInfo {callId, streamId};
    }
    frame->attach(this);
}

void
VideoMixer::detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame)
{
    if (!frame)
        return;
    bool detach = false;
    std::unique_lock lk(videoToStreamInfoMtx_);
    auto it = videoToStreamInfo_.find(frame);
    if (it != videoToStreamInfo_.end()) {
        JAMI_LOG("Detaching video of call {}", it->second.callId);
        detach = true;
        // Handle the case where the current shown source leave the conference
        // Note, do not call resetActiveStream() to avoid multiple updates
        if (verifyActive(it->second.streamId))
            activeStream_ = {};
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
    JAMI_LOG("Add new source [{}]", fmt::ptr(src.get()));
    sources_.emplace_back(std::move(src));
    JAMI_DEBUG("Total sources: {:d}", sources_.size());
    updateLayout();
}

void
VideoMixer::detached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    std::unique_lock lock(rwMutex_);

    for (const auto& x : sources_) {
        if (x->source == ob) {
            JAMI_LOG("Remove source [{}]", fmt::ptr(x.get()));
            sources_.remove(x);
            JAMI_DEBUG("Total sources: {:d}", sources_.size());
            updateLayout();
            break;
        }
    }
}

void
VideoMixer::update(Observable<std::shared_ptr<MediaFrame>>* ob, const std::shared_ptr<MediaFrame>& frame_p)
{
    std::shared_lock lock(rwMutex_);

    for (const auto& x : sources_) {
        if (x->source == ob) {
#ifdef ENABLE_HWACCEL
            std::shared_ptr<VideoFrame> frame;
            try {
                frame = HardwareAccel::transferToMainMemory(*std::static_pointer_cast<VideoFrame>(frame_p),
                                                            AV_PIX_FMT_NV12);
                x->atomic_copy(*std::static_pointer_cast<VideoFrame>(frame));
            } catch (const std::runtime_error& e) {
                JAMI_ERROR("[mixer:{}] Accel failure: {}", id_, e.what());
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
    nextProcess_ += std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::duration<double>(1. / frameRate_));
    const auto delay = nextProcess_ - std::chrono::steady_clock::now();
    if (delay.count() > 0)
        std::this_thread::sleep_for(delay);

    if (dynamicFormat_)
        checkDynamicFormat();

    // Nothing to do.
    if (width_ == 0 or height_ == 0) {
        return;
    }

    VideoFrame& output = getNewFrame();
    try {
        output.reserve(format_, width_, height_);
    } catch (const std::bad_alloc& e) {
        JAMI_ERROR("[mixer:{}] VideoFrame::allocBuffer() failed", id_);
        return;
    }

    libav_utils::fillWithBlack(output.pointer());

    {
        std::lock_guard lk(audioOnlySourcesMtx_);
        std::shared_lock lock(rwMutex_);

        int i = 0;
        bool activeFound = false;
        bool needsUpdate = layoutUpdated_ > 0;
        bool successfullyRendered = audioOnlySources_.size() != 0 && sources_.size() == 0;
        std::vector<SourceInfo> sourcesInfo;
        sourcesInfo.reserve(sources_.size() + audioOnlySources_.size());
        // add all audioonlysources
        for (auto& [callId, streamId] : audioOnlySources_) {
            auto active = verifyActive(streamId);
            if (currentLayout_ != Layout::ONE_BIG or active) {
                sourcesInfo.emplace_back(SourceInfo {{}, 0, 0, 10, 10, false, callId, streamId});
            }
            if (currentLayout_ == Layout::ONE_BIG) {
                if (active)
                    successfullyRendered = true;
                else
                    sourcesInfo.emplace_back(SourceInfo {{}, 0, 0, 0, 0, false, callId, streamId});
                // Add all participants info even in ONE_BIG layout.
                // The width and height set to 0 here will led the peer to filter them out.
            }
        }
        // add video sources
        for (auto& x : sources_) {
            /* thread stop pending? */
            if (!loop_.isRunning())
                return;

            auto sinfo = streamInfo(x->source);
            auto activeSource = verifyActive(sinfo.streamId);
            if (currentLayout_ != Layout::ONE_BIG or activeSource) {
                // make rendered frame temporarily unavailable for update()
                // to avoid concurrent access.
                std::shared_ptr<VideoFrame> input = x->getRenderFrame();
                std::shared_ptr<VideoFrame> fooInput = std::make_shared<VideoFrame>();

                auto wantedIndex = i;
                if (currentLayout_ == Layout::ONE_BIG) {
                    wantedIndex = 0;
                    activeFound = true;
                } else if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL) {
                    if (activeSource) {
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
                    updateLayout();
                    needsUpdate = true;
                }

                if (needsUpdate)
                    calc_position(x, fooInput, wantedIndex);

                if (!blackFrame) {
                    if (fooInput)
                        successfullyRendered |= render_frame(output, fooInput, x);
                    else
                        JAMI_WARNING("[mixer:{}] Nothing to render for {}", id_, fmt::ptr(x->source));
                }

                x->hasVideo = !blackFrame && successfullyRendered;
                if (hasVideo != x->hasVideo) {
                    updateLayout();
                    needsUpdate = true;
                }
            } else if (needsUpdate) {
                x->x = 0;
                x->y = 0;
                x->w = 0;
                x->h = 0;
                x->hasVideo = false;
            }

            ++i;
        }
        if (needsUpdate and successfullyRendered) {
            layoutUpdated_ -= 1;
            if (layoutUpdated_ == 0) {
                for (auto& x : sources_) {
                    auto sinfo = streamInfo(x->source);
                    sourcesInfo.emplace_back(
                        SourceInfo {x->source, x->x, x->y, x->w, x->h, x->hasVideo, sinfo.callId, sinfo.streamId});
                }
                if (onSourcesUpdated_)
                    onSourcesUpdated_(std::move(sourcesInfo));
            }
        }
    }

    output.pointer()->pts = av_rescale_q_rnd(av_gettime() - startTime_,
                                             {1, AV_TIME_BASE},
                                             {1, frameRate_},
                                             static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
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
        source->rotationFilter
            = video::getTransposeFilter(angle, filterIn, input->width(), input->height(), input->format(), false);
        source->rotation = angle;
    }
    std::shared_ptr<VideoFrame> frame;
    if (source->rotationFilter) {
        source->rotationFilter->feedInput(input->pointer(), filterIn);
        frame = std::static_pointer_cast<VideoFrame>(std::shared_ptr<MediaFrame>(source->rotationFilter->readOutput()));
    } else {
        frame = input;
    }

    scaler_.scale_and_pad(*frame, output, xoff, yoff, cell_width, cell_height, true);
    return true;
}

void
VideoMixer::calc_position(std::unique_ptr<VideoMixerSource>& source, const std::shared_ptr<VideoFrame>& input, int index)
{
    if (!width_ or !height_)
        return;

    // Compute cell size/position
    int cell_width, cell_height, cellW_off, cellH_off;
    const int n = currentLayout_ == Layout::ONE_BIG ? 1 : static_cast<int>(sources_.size());
    const int zoom = currentLayout_ == Layout::ONE_BIG_WITH_SMALL ? std::max(MIN_LINE_ZOOM, n)
                                                                  : static_cast<int>(ceil(sqrt(n)));
    if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL && index == 0) {
        // In ONE_BIG_WITH_SMALL, the first line at the top is the previews
        // The rest is the active source
        cell_width = width_;
        cell_height = height_ - height_ / zoom;
    } else {
        cell_width = width_ / zoom;
        cell_height = height_ / zoom;

        if (n == 1) {
            // On some platforms (at least macOS/android) - Having one frame at the same
            // size of the mixer cause it to be grey.
            // Removing some pixels solve this. We use 16 because it's a multiple of 8
            // (value that we prefer for video management)
            cell_width -= 16;
            cell_height -= 16;
        }
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
        if (n == 1) {
            // Centerize (cellwidth = width_ - 16)
            cellW_off += 8;
            cellH_off += 8;
        }
    }

    // Compute frame size/position
    int frameW, frameH, frameW_off, frameH_off;
    float zoomW, zoomH, denom;

    float inputW = static_cast<float>(input->width());
    float inputH = static_cast<float>(input->height());

    if (input->getOrientation() % 180) {
        // Rotated frame
        zoomW = inputH / static_cast<float>(cell_width);
        zoomH = inputW / static_cast<float>(cell_height);
        denom = std::max(zoomW, zoomH);
        frameH = static_cast<int>(std::lround(inputW / denom));
        frameW = static_cast<int>(std::lround(inputH / denom));
    } else {
        zoomW = inputW / static_cast<float>(cell_width);
        zoomH = inputH / static_cast<float>(cell_height);
        denom = std::max(zoomW, zoomH);
        frameW = static_cast<int>(std::lround(inputW / denom));
        frameH = static_cast<int>(std::lround(inputH / denom));
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
    updateLayout();
    startTime_ = av_gettime();
}

void
VideoMixer::startSink()
{
    stopSink();

    if (width_ == 0 or height_ == 0) {
        JAMI_WARNING("[mixer:{}] MX: unable to start with zero-sized output", id_);
        return;
    }

    if (not sink_->start()) {
        JAMI_ERROR("[mixer:{}] MX: sink startup failed", id_);
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
    std::shared_lock lock(rwMutex_);
    return width_;
}

int
VideoMixer::getHeight() const
{
    std::shared_lock lock(rwMutex_);
    return height_;
}

int
VideoMixer::getFrameRate() const
{
    std::shared_lock lock(rwMutex_);
    return frameRate_;
}

AVPixelFormat
VideoMixer::getPixelFormat() const
{
    return format_;
}

std::pair<int, int>
VideoMixer::computeTargetSurface(Layout layout,
                                 const std::vector<SourceSpec>& sources,
                                 std::pair<int, int> baseCap,
                                 std::pair<int, int> bigCap)
{
    const size_t n = sources.size();
    if (n == 0)
        return {0, 0};

    if (layout == Layout::ONE_BIG || layout == Layout::ONE_BIG_WITH_SMALL) {
        // The surface follows the promoted source (typically a screen share).
        // The large cap is only allowed while the conference stays small: the
        // composite is encoded once per participant.
        const auto* active = findActiveSource(sources);
        if (!active)
            return {0, 0};
        const auto& cap = (n <= MAX_BIG_CAP_SOURCES) ? bigCap : baseCap;
        auto target = clampSurface(active->width, active->height, cap);
        if (layout == Layout::ONE_BIG_WITH_SMALL)
            target = growForPreviews(target, sources, baseCap);
        return target;
    }

    // GRID: give each cell what the best source can fill, up to 720p per cell.
    const auto [cellW, cellH] = largestCell(sources, false);
    if (cellW <= 0 || cellH <= 0)
        return {0, 0};
    const int zoom = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
    return clampSurface(zoom * cellW, zoom * cellH, baseCap);
}

int
VideoMixer::computeTargetFrameRate(const std::vector<SourceSpec>& sources, int maxFrameRate)
{
    int fastest = 0;
    for (const auto& s : sources)
        fastest = std::max(fastest, s.frameRate);
    // Quantize to the standard rates so that measurement jitter (a 30 fps
    // camera measured at 28-32) never triggers a format change.
    const int target = fastest > 45 ? 60 : MIXER_FRAMERATE;
    return std::clamp(target, MIXER_FRAMERATE, std::max(MIXER_FRAMERATE, maxFrameRate));
}

void
VideoMixer::enableDynamicFormat(std::pair<int, int> baseCap, std::pair<int, int> bigCap, int maxFrameRate)
{
    baseCap_ = baseCap;
    bigCap_ = bigCap;
    maxFrameRate_ = maxFrameRate;
    nextFormatCheck_ = std::chrono::steady_clock::now() + FORMAT_CHECK_PERIOD;
    lastFormatChange_ = std::chrono::steady_clock::now();
    dynamicFormat_ = true;
    JAMI_LOG("[mixer:{}] Dynamic format enabled: base {}x{}, big {}x{}, max {} fps",
             id_,
             baseCap.first,
             baseCap.second,
             bigCap.first,
             bigCap.second,
             maxFrameRate);
}

void
VideoMixer::checkDynamicFormat()
{
    const auto now = std::chrono::steady_clock::now();
    if (now < nextFormatCheck_)
        return;
    const auto elapsed = std::chrono::duration<double>(now - (nextFormatCheck_ - FORMAT_CHECK_PERIOD)).count();
    nextFormatCheck_ = now + FORMAT_CHECK_PERIOD;

    std::vector<SourceSpec> specs;
    {
        std::shared_lock lock(rwMutex_);
        specs.reserve(sources_.size());
        for (auto& x : sources_) {
            const auto frames = x->framesSinceCheck.exchange(0, std::memory_order_relaxed);
            if (elapsed > 0.5)
                x->measuredFps = static_cast<int>(std::lround(frames / elapsed));
            auto frame = x->getRenderFrame();
            const bool active = verifyActive(streamInfo(x->source).streamId);
            if (frame && frame->width() > 0 && frame->height() > 0)
                specs.push_back(SourceSpec {frame->width(), frame->height(), x->measuredFps, active});
            else
                specs.push_back(SourceSpec {0, 0, x->measuredFps, active});
        }
    }

    if (now - lastFormatChange_ < MIN_FORMAT_CHANGE_INTERVAL)
        return;

    const auto target = computeTargetSurface(currentLayout_, specs, baseCap_, bigCap_);
    const auto targetFps = computeTargetFrameRate(specs, maxFrameRate_);

    bool surfaceChanged = false;
    if (target.first > 0 && target.second > 0 && width_ > 0 && height_ > 0) {
        const double ratio = static_cast<double>(target.first) * target.second
                             / (static_cast<double>(width_) * height_);
        surfaceChanged = ratio >= GROW_AREA_RATIO || ratio <= SHRINK_AREA_RATIO;
    }
    // Measured frame rates are quantized to standard rates, so a plain
    // comparison is stable against measurement jitter.
    const bool fpsChanged = targetFps != frameRate_;
    if (!surfaceChanged && !fpsChanged)
        return;

    JAMI_LOG("[mixer:{}] Format change: {}x{}@{} -> {}x{}@{}",
             id_,
             width_,
             height_,
             frameRate_,
             surfaceChanged ? target.first : width_,
             surfaceChanged ? target.second : height_,
             targetFps);
    if (fpsChanged) {
        // Re-anchor the pacing schedule so the next frame follows the new
        // rate instead of an accumulated deadline from the previous one.
        nextProcess_ = std::chrono::steady_clock::now();
    }
    {
        // Readers (getStream and the getters) take the same lock: keep the
        // published width/height/frameRate tuple consistent.
        std::unique_lock lock(rwMutex_);
        frameRate_ = targetFps;
    }
    if (surfaceChanged)
        setParameters(target.first, target.second, format_);
    lastFormatChange_ = std::chrono::steady_clock::now();
    if (onFormatChanged_)
        onFormatChanged_(width_, height_, frameRate_);
}

MediaStream
VideoMixer::getStream(const std::string& name) const
{
    std::shared_lock lock(rwMutex_);
    MediaStream ms;
    ms.name = name;
    ms.format = format_;
    ms.isVideo = true;
    ms.height = height_;
    ms.width = width_;
    ms.frameRate = {frameRate_, 1};
    ms.timeBase = {1, frameRate_};
    ms.firstTimestamp = lastTimestamp_;

    return ms;
}

} // namespace video
} // namespace jami
