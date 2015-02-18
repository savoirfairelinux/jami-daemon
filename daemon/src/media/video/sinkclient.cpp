/*
 *  Copyright (C) 2012-2015 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  Portions derived from GStreamer:
 *  Copyright (C) <2009> Collabora Ltd
 *  @author: Olivier Crete <olivier.crete@collabora.co.uk
 *  Copyright (C) <2009> Nokia Inc
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sinkclient.h"

#if HAVE_SHM
#include "shm_header.h"
#endif // HAVE_SHM

#include "video_scaler.h"
#include "media_buffer.h"
#include "logger.h"
#include "noncopyable.h"

#include <sys/mman.h>
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
        ShmHolder(const std::string& shm_name={});
        ~ShmHolder();

        std::string openedName() const noexcept {
            return opened_name_;
        }

        void render_frame(VideoFrame& src);

    private:
        bool resize_area(std::size_t desired_length);
        void alloc_area(std::size_t desired_length) noexcept;

        std::string shm_name_;
        std::string opened_name_;
        std::size_t shm_area_len_ {0};
        SHMHeader* shm_area_ {static_cast<SHMHeader*>(MAP_FAILED)};
        int fd_ {-1};
};

void
ShmHolder::alloc_area(std::size_t desired_length) noexcept
{
    shm_area_ = static_cast<SHMHeader*>(::mmap(nullptr, desired_length,
                                               PROT_READ | PROT_WRITE,
                                               MAP_SHARED, fd_, 0));
}

ShmHolder::ShmHolder(const std::string& shm_name) : shm_name_ {shm_name}
{
    static constexpr int flags = O_RDWR | O_CREAT | O_TRUNC | O_EXCL;
    static constexpr int perms = S_IRUSR | S_IWUSR;

    if (not shm_name_.empty()) {
        fd_ = ::shm_open(shm_name_.c_str(), flags, perms);
        if (fd_ < 0) {
            std::ostringstream msg;
            msg << "could not open shm area \""
                << shm_name_.c_str()
                << "\"";
            throw std::runtime_error {msg.str()};
        }
    } else {
        for (int i = 0; fd_ < 0; ++i) {
            std::ostringstream name;
            name << PACKAGE_NAME << "_shm_" << getpid() << "_" << i;
            shm_name_ = name.str();
            fd_ = ::shm_open(shm_name_.c_str(), flags, perms);
            if (fd_ < 0 and errno != EEXIST)
                throw std::runtime_error {"shm_open() failed"};
        }
    }

    RING_DBG("Using name %s", shm_name_.c_str());
    opened_name_ = shm_name_;

    shm_area_len_ = sizeof(SHMHeader);

    if (::ftruncate(fd_, shm_area_len_)) {
        RING_ERR("Could not make shm area large enough for header");
        strErr();
        throw std::runtime_error {"could not make shm area large enough for header"};
    }

    alloc_area(shm_area_len_);

    if (shm_area_ == MAP_FAILED)
        throw std::runtime_error {"could not map shm area, mmap failed"};

    std::memset(shm_area_, 0, shm_area_len_);

    if (::sem_init(&shm_area_->notification, 1, 0) != 0)
        throw std::runtime_error {"sem_init: notification initialization failed"};

    if (::sem_init(&shm_area_->mutex, 1, 1) != 0)
        throw std::runtime_error {"sem_init: mutex initialization failed"};
}

ShmHolder::~ShmHolder()
{
    if (fd_ >= 0 and ::close(fd_) == -1)
        strErr();

    if (not opened_name_.empty())
        ::shm_unlink(opened_name_.c_str());

    if (shm_area_ != MAP_FAILED) {
        ::sem_post(&shm_area_->notification);
        if (::munmap(shm_area_, shm_area_len_) < 0)
            strErr();
    }
}

bool
ShmHolder::resize_area(size_t desired_length)
{
    if (desired_length <= shm_area_len_)
        return true;

    if (::munmap(shm_area_, shm_area_len_)) {
        RING_ERR("Could not unmap shared area");
        strErr();
        return false;
    }

    if (::ftruncate(fd_, desired_length)) {
        RING_ERR("Could not resize shared area");
        strErr();
        return false;
    }

    alloc_area(desired_length);

    if (shm_area_ == MAP_FAILED) {
        shm_area_len_ = 0;
        RING_ERR("Could not remap shared area");
        return false;
    }

    shm_area_len_ = desired_length;
    return true;
}

void
ShmHolder::render_frame(VideoFrame& src)
{
    VideoFrame dst;
    VideoScaler scaler;

    const int width = src.width();
    const int height = src.height();
    const int format = VIDEO_PIXFMT_BGRA;
    const auto bytes = videoFrameSize(format, width, height);

    if (!resize_area(sizeof(SHMHeader) + bytes)) {
        RING_ERR("Could not resize area");
        return;
    }

    SemGuardLock lk{shm_area_->mutex};

    dst.setFromMemory(shm_area_->data, format, width, height);
    scaler.scale(src, dst);

    shm_area_->buffer_size = bytes;
    ++shm_area_->buffer_gen;
    sem_post(&shm_area_->notification);
}

std::string
SinkClient::openedName() const noexcept
{
    return shm_->openedName();
}

bool
SinkClient::start() noexcept
{
    if (not shm_) {
        try {
            shm_ = std::make_shared<ShmHolder>();
        } catch (const std::runtime_error& e) {
            strErr();
            RING_ERR("SHMHolder ctor failure: %s", e.what());
        }
    }
    return static_cast<bool>(shm_);
}

bool
SinkClient::stop() noexcept
{
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
    return true;
}

#endif // !HAVE_SHM

SinkClient::SinkClient(const std::string& id) : id_ {id}
#ifdef DEBUG_FPS
    , frameCount_(0u)
    , lastFrameDebug_(std::chrono::system_clock::now())
#endif
{}

void
SinkClient::update(Observable<std::shared_ptr<VideoFrame>>* /*obs*/,
                   std::shared_ptr<VideoFrame>& frame_p)
{
    auto f = frame_p; // keep a local reference during rendering

#ifdef DEBUG_FPS
    auto currentTime = std::chrono::system_clock::now();
    const std::chrono::duration<double> seconds = currentTime - lastFrameDebug_;
    ++frameCount_;
    if (seconds.count() > 1) {
        RING_DBG("%s: FPS %f", id_.c_str(), frameCount_ / seconds.count());
        frameCount_ = 0;
        lastFrameDebug_ = currentTime;
    }
#endif

#if HAVE_SHM
    shm_->render_frame(*f.get());
#endif

    if (target_) {
        VideoFrame dst;
        VideoScaler scaler;
        const int width = f->width();
        const int height = f->height();
        const int format = VIDEO_PIXFMT_BGRA;
        const auto bytes = videoFrameSize(format, width, height);

        targetData_.resize(bytes);
        auto data = targetData_.data();

        dst.setFromMemory(data, format, width, height);
        scaler.scale(*f, dst);
        target_(data);
    }
}

}} // namespace ring::video
