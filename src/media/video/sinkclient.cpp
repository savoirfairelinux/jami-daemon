/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sinkclient.h"

#ifdef ENABLE_SHM
#include "shm_header.h"
#endif // ENABLE_SHM

#include "media_buffer.h"
#include "logger.h"
#include "noncopyable.h"
#include "client/ring_signal.h"
#include "jami/videomanager_interface.h"
#include "libav_utils.h"
#include "video_scaler.h"
#include "media_filter.h"
#include "filter_transpose.h"

#ifdef RING_ACCEL
#include "accel.h"
#endif

#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <ciso646> // fix windows compiler bug
#include <fcntl.h>
#include <cstdio>
#include <sstream>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <cmath>

namespace jami {
namespace video {

const constexpr char FILTER_INPUT_NAME[] = "in";

#ifdef ENABLE_SHM
// RAII class helper on sem_wait/sem_post sempahore operations
class SemGuardLock
{
public:
    explicit SemGuardLock(sem_t& mutex)
        : m_(mutex)
    {
        auto ret = ::sem_wait(&m_);
        if (ret < 0) {
            std::ostringstream msg;
            msg << "SHM mutex@" << &m_ << " lock failed (" << ret << ")";
            throw std::logic_error {msg.str()};
        }
    }

    ~SemGuardLock() { ::sem_post(&m_); }

private:
    sem_t& m_;
};

class ShmHolder
{
public:
    ShmHolder(const std::string& name = {});
    ~ShmHolder();

    std::string name() const noexcept { return openedName_; }

    void renderFrame(const VideoFrame& src) noexcept;

private:
    bool resizeArea(std::size_t desired_length) noexcept;
    char* getShmAreaDataPtr() noexcept;

    void unMapShmArea() noexcept
    {
        if (area_ != MAP_FAILED and ::munmap(area_, areaSize_) < 0) {
            JAMI_ERR("[ShmHolder:%s] munmap(%zu) failed with errno %d",
                     openedName_.c_str(),
                     areaSize_,
                     errno);
        }
    }

    SHMHeader* area_ {static_cast<SHMHeader*>(MAP_FAILED)};
    std::size_t areaSize_ {0};
    std::string openedName_;
    int fd_ {-1};
};

ShmHolder::ShmHolder(const std::string& name)
{
    static constexpr int flags = O_RDWR | O_CREAT | O_TRUNC | O_EXCL;
    static constexpr int perms = S_IRUSR | S_IWUSR;

    static auto shmFailedWithErrno = [this](const std::string& what) {
        std::ostringstream msg;
        msg << "ShmHolder[" << openedName_ << "]: " << what << " failed, errno=" << errno;
        throw std::runtime_error {msg.str()};
    };

    if (not name.empty()) {
        openedName_ = name;
        fd_ = ::shm_open(openedName_.c_str(), flags, perms);
        if (fd_ < 0)
            shmFailedWithErrno("shm_open");
    } else {
        for (int i = 0; fd_ < 0; ++i) {
            std::ostringstream tmpName;
            tmpName << PACKAGE_NAME << "_shm_" << getpid() << "_" << i;
            openedName_ = tmpName.str();
            fd_ = ::shm_open(openedName_.c_str(), flags, perms);
            if (fd_ < 0 and errno != EEXIST)
                shmFailedWithErrno("shm_open");
        }
    }

    // Set size enough for header only (no frame data)
    if (!resizeArea(0))
        shmFailedWithErrno("resizeArea");

    // Header fields initialization
    std::memset(area_, 0, areaSize_);

    if (::sem_init(&area_->mutex, 1, 1) < 0)
        shmFailedWithErrno("sem_init(mutex)");

    if (::sem_init(&area_->frameGenMutex, 1, 0) < 0)
        shmFailedWithErrno("sem_init(frameGenMutex)");

    JAMI_DBG("[ShmHolder:%s] New holder created", openedName_.c_str());
}

ShmHolder::~ShmHolder()
{
    if (fd_ < 0)
        return;

    ::close(fd_);
    ::shm_unlink(openedName_.c_str());

    if (area_ == MAP_FAILED)
        return;

    ::sem_wait(&area_->mutex);
    area_->frameSize = 0;
    ::sem_post(&area_->mutex);

    ::sem_post(&area_->frameGenMutex); // unlock waiting client before leaving
    unMapShmArea();
}

bool
ShmHolder::resizeArea(std::size_t frameSize) noexcept
{
    // aligned on 16-byte boundary frameSize
    frameSize = (frameSize + 15) & ~15;

    if (area_ != MAP_FAILED and frameSize == area_->frameSize)
        return true;

    // full area size: +15 to take care of maximum padding size
    const auto areaSize = sizeof(SHMHeader) + 2 * frameSize + 15;
    JAMI_DBG("[ShmHolder:%s] New size: f=%zu, a=%zu", openedName_.c_str(), frameSize, areaSize);

    unMapShmArea();

    if (::ftruncate(fd_, areaSize) < 0) {
        JAMI_ERR("[ShmHolder:%s] ftruncate(%zu) failed with errno %d",
                 openedName_.c_str(),
                 areaSize,
                 errno);
        return false;
    }

    area_ = static_cast<SHMHeader*>(
        ::mmap(nullptr, areaSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));

    if (area_ == MAP_FAILED) {
        areaSize_ = 0;
        JAMI_ERR("[ShmHolder:%s] mmap(%zu) failed with errno %d",
                 openedName_.c_str(),
                 areaSize,
                 errno);
        return false;
    }

    areaSize_ = areaSize;

    if (frameSize) {
        SemGuardLock lk {area_->mutex};

        area_->frameSize = frameSize;
        area_->mapSize = areaSize;

        // Compute aligned IO pointers
        // Note: we not using std::align as not implemented in 4.9
        // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57350
        auto p = reinterpret_cast<std::uintptr_t>(area_->data);
        area_->writeOffset = ((p + 15) & ~15) - p;
        area_->readOffset = area_->writeOffset + frameSize;
    }

    return true;
}

void
ShmHolder::renderFrame(const VideoFrame& src) noexcept
{
    const auto width = src.width();
    const auto height = src.height();
    const auto format = AV_PIX_FMT_BGRA;
    const auto frameSize = videoFrameSize(format, width, height);

    if (!resizeArea(frameSize)) {
        JAMI_ERR("[ShmHolder:%s] Could not resize area size: %dx%d, format: %d",
                 openedName_.c_str(),
                 width,
                 height,
                 format);
        return;
    }

    {
        VideoFrame dst;
        VideoScaler scaler;

        dst.setFromMemory(area_->data + area_->writeOffset, format, width, height);
        scaler.scale(src, dst);
    }

    {
        SemGuardLock lk {area_->mutex};

        ++area_->frameGen;
        std::swap(area_->readOffset, area_->writeOffset);
        ::sem_post(&area_->frameGenMutex);
    }
}

std::string
SinkClient::openedName() const noexcept
{
    if (shm_)
        return shm_->name();
    return {};
}

bool
SinkClient::start() noexcept
{
    if (not shm_) {
        try {
            char* envvar = getenv("JAMI_DISABLE_SHM");
            if (envvar) // Do not use SHM if set
                return true;
            shm_ = std::make_shared<ShmHolder>();
            JAMI_DBG("[Sink:%p] Shared memory [%s] created", this, openedName().c_str());
            if (width_ > 0 && height_ > 0) {
                emitSignal<libjami::VideoSignal::DecodingStarted>(getId(),
                                                                openedName(),
                                                                width_,
                                                                height_,
                                                                mixer_);
            }
        } catch (const std::runtime_error& e) {
            JAMI_ERR("[Sink:%p] Failed to create shared memory: %s", this, e.what());
        }
    }

    return static_cast<bool>(shm_);
}

bool
SinkClient::stop() noexcept
{
    setFrameSize(0, 0);
    setCrop(0, 0, 0, 0);
    shm_.reset();
    return true;
}

#else // ENABLE_SHM

std::string
SinkClient::openedName() const noexcept
{
    return {};
}

bool
SinkClient::start() noexcept
{
    return true;
}

bool
SinkClient::stop() noexcept
{
    setFrameSize(0, 0);
    setCrop(0, 0, 0, 0);
    return true;
}

#endif // !ENABLE_SHM

SinkClient::SinkClient(const std::string& id, bool mixer)
    : id_ {id}
    , mixer_(mixer)
    , scaler_(new VideoScaler())
#ifdef DEBUG_FPS
    , frameCount_(0u)
    , lastFrameDebug_(std::chrono::steady_clock::now())
#endif
{
    JAMI_DBG("[Sink:%p] Sink [%s] created", this, getId().c_str());
}

void
SinkClient::sendFrameDirect(const std::shared_ptr<jami::MediaFrame>& frame_p)
{
    notify(frame_p);

    libjami::FrameBuffer outFrame(av_frame_alloc());
    av_frame_ref(outFrame.get(), std::static_pointer_cast<VideoFrame>(frame_p)->pointer());

    if (crop_.w || crop_.h) {
#ifdef RING_ACCEL
        auto desc = av_pix_fmt_desc_get(
            (AVPixelFormat) std::static_pointer_cast<VideoFrame>(frame_p)->format());
        /*
         Cropping does not work for hardware-decoded frames.
         They need to be transferred to main memory.
         */
        if (desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            std::shared_ptr<VideoFrame> frame = std::make_shared<VideoFrame>();
            try {
                frame = HardwareAccel::transferToMainMemory(*std::static_pointer_cast<VideoFrame>(
                                                                frame_p),
                                                            AV_PIX_FMT_NV12);
            } catch (const std::runtime_error& e) {
                JAMI_ERR("[Sink:%p] Transfert to hardware acceleration memory failed: %s",
                         this,
                         e.what());
                return;
            }
            if (not frame)
                return;
            av_frame_unref(outFrame.get());
            av_frame_ref(outFrame.get(), frame->pointer());
        }
#endif
        outFrame->crop_top = crop_.y;
        outFrame->crop_bottom = (size_t) outFrame->height - crop_.y - crop_.h;
        outFrame->crop_left = crop_.x;
        outFrame->crop_right = (size_t) outFrame->width - crop_.x - crop_.w;
        av_frame_apply_cropping(outFrame.get(), AV_FRAME_CROP_UNALIGNED);
    }
    if (outFrame->height != height_ || outFrame->width != width_) {
        setFrameSize(outFrame->width, outFrame->height);
        return;
    }
    target_.push(std::move(outFrame));
}

void
SinkClient::sendFrameTransformed(AVFrame* frame)
{
    if (frame->width > 0 and frame->height > 0) {
        if (auto buffer_ptr = target_.pull()) {
            scaler_->scale(frame, buffer_ptr.get());
            target_.push(std::move(buffer_ptr));
        }
    }
}

std::shared_ptr<VideoFrame>
SinkClient::applyTransform(VideoFrame& frame_p)
{
    std::shared_ptr<VideoFrame> frame = std::make_shared<VideoFrame>();
#ifdef RING_ACCEL
    auto desc = av_pix_fmt_desc_get((AVPixelFormat) frame_p.format());
    if (desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        try {
            frame = HardwareAccel::transferToMainMemory(frame_p, AV_PIX_FMT_NV12);
        } catch (const std::runtime_error& e) {
            JAMI_ERR("[Sink:%p] Transfert to hardware acceleration memory failed: %s",
                     this,
                     e.what());
            return {};
        }
    } else
#endif
        frame->copyFrom(frame_p);

    int angle = frame->getOrientation();
    if (angle != rotation_) {
        filter_ = getTransposeFilter(angle,
                                     FILTER_INPUT_NAME,
                                     frame->width(),
                                     frame->height(),
                                     frame->format(),
                                     false);
        rotation_ = angle;
    }
    if (filter_) {
        filter_->feedInput(frame->pointer(), FILTER_INPUT_NAME);
        frame = std::static_pointer_cast<VideoFrame>(
            std::shared_ptr<MediaFrame>(filter_->readOutput()));
    }
    if (crop_.w || crop_.h) {
        frame->pointer()->crop_top = crop_.y;
        frame->pointer()->crop_bottom = (size_t) frame->height() - crop_.y - crop_.h;
        frame->pointer()->crop_left = crop_.x;
        frame->pointer()->crop_right = (size_t) frame->width() - crop_.x - crop_.w;
        av_frame_apply_cropping(frame->pointer(), AV_FRAME_CROP_UNALIGNED);
    }
    return frame;
}

void
SinkClient::update(Observable<std::shared_ptr<MediaFrame>>* /*obs*/,
                   const std::shared_ptr<MediaFrame>& frame_p)
{
#ifdef DEBUG_FPS
    auto currentTime = std::chrono::steady_clock::now();
    auto seconds = currentTime - lastFrameDebug_;
    ++frameCount_;
    if (seconds > std::chrono::seconds(1)) {
        auto fps = frameCount_ / std::chrono::duration<double>(seconds).count();
        JAMI_WARNING("Sink {}, {} FPS", id_, fps);
        frameCount_ = 0;
        lastFrameDebug_ = currentTime;
    }
#endif

    std::unique_lock<std::mutex> lock(mtx_);
    bool hasObservers = getObserversCount() != 0;
    bool hasDirectListener = target_.push and not target_.pull;
    bool hasTransformedListener = target_.push and target_.pull;

    if (hasDirectListener) {
        sendFrameDirect(frame_p);
        return;
    }

    bool doTransfer = hasTransformedListener or hasObservers;
#ifdef ENABLE_SHM
    doTransfer |= (shm_ && doShmTransfer_);
#endif

    if (doTransfer) {
        auto frame = applyTransform(*std::static_pointer_cast<VideoFrame>(frame_p));
        if (not frame)
            return;

        notify(std::static_pointer_cast<MediaFrame>(frame));

        if (frame->height() != height_ || frame->width() != width_) {
            lock.unlock();
            setFrameSize(frame->width(), frame->height());
            return;
        }
#ifdef ENABLE_SHM
        if (shm_ && doShmTransfer_)
            shm_->renderFrame(*frame);
#endif
        if (hasTransformedListener)
            sendFrameTransformed(frame->pointer());
    }
}

void
SinkClient::setFrameSize(int width, int height)
{
    width_ = width;
    height_ = height;
    if (width > 0 and height > 0) {
        JAMI_DBG("[Sink:%p] Started - size=%dx%d, mixer=%s",
                 this,
                 width,
                 height,
                 mixer_ ? "Yes" : "No");
        emitSignal<libjami::VideoSignal::DecodingStarted>(getId(),
                                                          openedName(),
                                                          width,
                                                          height,
                                                          mixer_);
        started_ = true;
    } else if (started_) {
        JAMI_DBG("[Sink:%p] Stopped - size=%dx%d, mixer=%s",
                 this,
                 width,
                 height,
                 mixer_ ? "Yes" : "No");
        emitSignal<libjami::VideoSignal::DecodingStopped>(getId(), openedName(), mixer_);
        started_ = false;
    }
}

void
SinkClient::setCrop(int x, int y, int w, int h)
{
    JAMI_DBG("[Sink:%p] Change crop to [%dx%d at (%d, %d)]", this, w, h, x, y);
    crop_.x = x;
    crop_.y = y;
    crop_.w = w;
    crop_.h = h;
}

} // namespace video
} // namespace jami
