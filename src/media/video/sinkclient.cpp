/*
 *  Copyright (C) 2012-2019 Savoir-faire Linux Inc.
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

#if HAVE_SHM
#include "shm_header.h"
#endif // HAVE_SHM

#include "media_buffer.h"
#include "logger.h"
#include "noncopyable.h"
#include "client/ring_signal.h"
#include "dring/videomanager_interface.h"
#include "libav_utils.h"
#include "video_scaler.h"
#include "smartools.h"

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

namespace ring { namespace video {

#if HAVE_SHM
// RAII class helper on sem_wait/sem_post sempahore operations
class SemGuardLock {
    public:
        explicit SemGuardLock(sem_t& mutex) : m_(mutex) {
            auto ret = ::sem_wait(&m_);
            if (ret < 0) {
                std::ostringstream msg;
                msg << "SHM mutex@" << &m_
                    << " lock failed (" << ret << ")";
                throw std::logic_error {msg.str()};
            }
        }

        ~SemGuardLock() {
            ::sem_post(&m_);
        }

    private:
        sem_t& m_;
};

class ShmHolder
{
    public:
        ShmHolder(const std::string& name={});
        ~ShmHolder();

        std::string name() const noexcept {
            return openedName_;
        }

        void renderFrame(VideoFrame& src) noexcept;

    private:
        bool resizeArea(std::size_t desired_length) noexcept;
        char* getShmAreaDataPtr() noexcept;

        void unMapShmArea() noexcept {
            if (area_ != MAP_FAILED and ::munmap(area_, areaSize_) < 0) {
                RING_ERR("ShmHolder[%s]: munmap(%zu) failed with errno %d",
                         openedName_.c_str(), areaSize_, errno);
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
        msg << "ShmHolder[" << openedName_ << "]: "
        << what << " failed, errno=" << errno;
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

    RING_DBG("ShmHolder: new holder '%s'", openedName_.c_str());
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
    RING_DBG("ShmHolder[%s]: new sizes: f=%zu, a=%zu", openedName_.c_str(),
             frameSize, areaSize);

    unMapShmArea();

    if (::ftruncate(fd_, areaSize) < 0) {
        RING_ERR("ShmHolder[%s]: ftruncate(%zu) failed with errno %d",
                 openedName_.c_str(), areaSize, errno);
        return false;
    }

    area_ = static_cast<SHMHeader*>(::mmap(nullptr, areaSize,
                                           PROT_READ | PROT_WRITE,
                                           MAP_SHARED, fd_, 0));

    if (area_ == MAP_FAILED) {
        areaSize_ = 0;
        RING_ERR("ShmHolder[%s]: mmap(%zu) failed with errno %d",
                 openedName_.c_str(), areaSize, errno);
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
ShmHolder::renderFrame(VideoFrame& src) noexcept
{
    const auto width = src.width();
    const auto height = src.height();
    const auto format = AV_PIX_FMT_BGRA;
    const auto frameSize = videoFrameSize(format, width, height);

    if (!resizeArea(frameSize)) {
        RING_ERR("ShmHolder[%s]: could not resize area",
                 openedName_.c_str());
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
            shm_ = std::make_shared<ShmHolder>();
        } catch (const std::runtime_error& e) {
            RING_ERR("SHMHolder ctor failure: %s", e.what());
        }
    }
    return static_cast<bool>(shm_);
}

bool
SinkClient::stop() noexcept
{
    setFrameSize(0, 0);
    shm_.reset();
    return true;
}

#else // HAVE_SHM

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
    return true;
}

#endif // !HAVE_SHM

SinkClient::SinkClient(const std::string& id, bool mixer)
    : id_ {id}
    , mixer_(mixer)
    , scaler_(new VideoScaler())
#ifdef DEBUG_FPS
    , frameCount_(0u)
    , lastFrameDebug_(std::chrono::system_clock::now())
#endif
{}

void
SinkClient::update(Observable<std::shared_ptr<MediaFrame>>* /*obs*/,
                   const std::shared_ptr<MediaFrame>& frame_p)
{
    auto& f = *std::static_pointer_cast<VideoFrame>(frame_p);

#ifdef DEBUG_FPS
    auto currentTime = std::chrono::system_clock::now();
    const std::chrono::duration<double> seconds = currentTime - lastFrameDebug_;
    ++frameCount_;
    if (seconds.count() > 1) {
        std::ostringstream fps;
        fps << frameCount_ / seconds.count();
        // Send the framerate in smartInfo
        Smartools::getInstance().setFrameRate(id_, fps.str());
        frameCount_ = 0;
        lastFrameDebug_ = currentTime;
    }
#endif

    if (avTarget_.push) {
        auto outFrame = std::make_unique<VideoFrame>();
        outFrame->copyFrom(f);
        avTarget_.push(std::move(outFrame));
    }

    bool doTransfer = (target_.pull != nullptr);
#if HAVE_SHM
    doTransfer |= (shm_ != nullptr);
#endif

    if (doTransfer) {
#ifdef RING_ACCEL
        auto swFrame = HardwareAccel::transferToMainMemory(f, AV_PIX_FMT_NV12);
#else
        std::unique_ptr<VideoFrame> swFrame;
        swFrame->copyFrom(f);
#endif
#if HAVE_SHM
        shm_->renderFrame(*swFrame);
#endif
        if (target_.pull) {
            VideoFrame dst;
            const int width = swFrame->width();
            const int height = swFrame->height();
#if defined(__ANDROID__) || (defined(__APPLE__) && !TARGET_OS_IPHONE)
            const int format = AV_PIX_FMT_RGBA;
#else
            const int format = AV_PIX_FMT_BGRA;
#endif
            const auto bytes = videoFrameSize(format, width, height);
            if (bytes > 0) {
                if (auto buffer_ptr = target_.pull(bytes)) {
                    buffer_ptr->format = format;
                    buffer_ptr->width = width;
                    buffer_ptr->height = height;
                    dst.setFromMemory(buffer_ptr->ptr, format, width, height);
                    scaler_->scale(*swFrame, dst);
                    target_.push(std::move(buffer_ptr));
                }
            }
        }
    }
}

void
SinkClient::setFrameSize(int width, int height)
{
    width_ = width;
    height_ = height;
    if (width > 0 and height > 0) {
        RING_WARN("Start sink <%s / %s>, size=%dx%d, mixer=%u",
                 getId().c_str(), openedName().c_str(), width, height, mixer_);
        emitSignal<DRing::VideoSignal::DecodingStarted>(getId(), openedName(), width, height, mixer_);
        started_ = true;
    } else if (started_) {
        RING_ERR("Stop sink <%s / %s>, mixer=%u",
                 getId().c_str(), openedName().c_str(), mixer_);
        emitSignal<DRing::VideoSignal::DecodingStopped>(getId(), openedName(), mixer_);
        started_ = false;
    }
}

}} // namespace ring::video
