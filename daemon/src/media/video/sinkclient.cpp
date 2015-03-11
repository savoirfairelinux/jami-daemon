/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
class ShmHolder
{
    public:
        ShmHolder(const std::string& shm_name);
        ~ShmHolder();

        const std::string& openedName() const noexcept {
            return opened_name_;
        }

        void render_frame(VideoFrame& src);

        bool start();
        bool stop();

    private:
        NON_COPYABLE(ShmHolder);

        bool resize_area(size_t desired_length);

        std::string shm_name_;
        int fd_;
        SHMHeader* shm_area_;
        size_t shm_area_len_;
        std::string opened_name_;
};

/* RAII helper to lock shm mutex */
class ShmLock {
    public:
        explicit ShmLock(sem_t& mutex) : m_(mutex) {
            auto ret = ::sem_wait(&m_);
            if (ret < 0) {
                std::ostringstream msg;
                msg << "SHM mutex@" << &m_
                    << " lock failed (" << ret << ")";
                throw std::logic_error {msg.str()};
            }
        }

        ~ShmLock() noexcept {
            ::sem_post(&m_);
        }

    private:
        sem_t& m_;
};

ShmHolder::ShmHolder(const std::string& shm_name)
    : shm_name_(shm_name)
    , fd_(-1)
    , shm_area_(static_cast<SHMHeader*>(MAP_FAILED))
    , shm_area_len_(0)
    , opened_name_()
{
}

ShmHolder::~ShmHolder()
{
    stop();
}

bool
ShmHolder::start()
{
    if (fd_ != -1) {
        RING_ERR("fd must be -1");
        return false;
    }

    const int flags = O_RDWR | O_CREAT | O_TRUNC | O_EXCL;
    const int perms = S_IRUSR | S_IWUSR;

    if (not shm_name_.empty()) {
        fd_ = ::shm_open(shm_name_.c_str(), flags, perms);
        if (fd_ < 0) {
            RING_ERR("could not open shm area \"%s\"", shm_name_.c_str());
            strErr();
            return false;
        }
    } else {
        for (int i = 0; fd_ < 0; ++i) {
            std::ostringstream name;
            name << PACKAGE_NAME << "_shm_" << getpid() << "_" << i;
            shm_name_ = name.str();
            fd_ = ::shm_open(shm_name_.c_str(), flags, perms);
            if (fd_ < 0 and errno != EEXIST) {
                strErr();
                return false;
            }
        }
    }

    RING_DBG("Using name %s", shm_name_.c_str());
    opened_name_ = shm_name_;

    shm_area_len_ = sizeof(SHMHeader);

    if (::ftruncate(fd_, shm_area_len_)) {
        RING_ERR("Could not make shm area large enough for header");
        strErr();
        return false;
    }

    shm_area_ = static_cast<SHMHeader*>(::mmap(NULL, shm_area_len_,
                                             PROT_READ | PROT_WRITE,
                                             MAP_SHARED, fd_, 0));

    if (shm_area_ == MAP_FAILED) {
        RING_ERR("Could not map shm area, mmap failed");
        return false;
    }

    std::memset(shm_area_, 0, shm_area_len_);
    if (::sem_init(&shm_area_->notification, 1, 0) != 0) {
        RING_ERR("sem_init: notification initialization failed");
        return false;
    }
    if (::sem_init(&shm_area_->mutex, 1, 1) != 0) {
        RING_ERR("sem_init: mutex initialization failed");
        return false;
    }
    return true;
}

bool
ShmHolder::stop()
{
    if (fd_ >= 0 and ::close(fd_) == -1)
        strErr();

    fd_ = -1;

    if (not opened_name_.empty()) {
        ::shm_unlink(opened_name_.c_str());
        opened_name_ = "";
    }

    if (shm_area_ != MAP_FAILED)
        ::munmap(shm_area_, shm_area_len_);
    shm_area_len_ = 0;
    shm_area_ = static_cast<SHMHeader*>(MAP_FAILED);

    return true;
}

bool
ShmHolder::resize_area(size_t desired_length)
{
    if (desired_length <= shm_area_len_)
        return true;

    ShmLock lk{shm_area_->mutex};

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

    shm_area_ = static_cast<SHMHeader*>(::mmap(NULL, desired_length,
                                               PROT_READ | PROT_WRITE,
                                               MAP_SHARED, fd_, 0));
    shm_area_len_ = desired_length;

    if (shm_area_ == MAP_FAILED) {
        shm_area_ = 0;
        RING_ERR("Could not remap shared area");
        return false;
    }

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
    size_t bytes = videoFrameSize(format, width, height);

    ShmLock lk{shm_area_->mutex};

    if (!resize_area(sizeof(SHMHeader) + bytes)) {
        RING_ERR("Could not resize area");
        return;
    }

    dst.setFromMemory(shm_area_->data, format, width, height);
    scaler.scale(src, dst);

    shm_area_->buffer_size = bytes;
    shm_area_->buffer_gen++;
    sem_post(&shm_area_->notification);
}

const std::string&
SinkClient::openedName() const noexcept {
    return shm_->openedName();
}

bool
SinkClient::start()
{
    return shm_->start();
}

bool
SinkClient::stop()
{
    return shm_->stop();
}

#else // HAVE_SHM

const std::string&
SinkClient::openedName() const noexcept {
    return {};
}

bool
SinkClient::start()
{
    return true;
}

bool
SinkClient::stop()
{
    return true;
}

#endif // !HAVE_SHM

SinkClient::SinkClient(const std::string& id) : id_(id)
{
#if HAVE_SHM
    shm_ = new ShmHolder {id};
#endif // HAVE_SHM
}

SinkClient::~SinkClient()
{
#if HAVE_SHM
    delete shm_;
#endif // HAVE_SHM
}

void
SinkClient::update(Observable<std::shared_ptr<VideoFrame> >* /*obs*/,
                   std::shared_ptr<VideoFrame> &frame_p)
{
    auto f = frame_p; // keep a local reference during rendering

#if HAVE_SHM
    shm_->render_frame(*f.get());
#endif

    if (target_) {
        VideoFrame dst;
        VideoScaler scaler;

        const int width = f->width();
        const int height = f->height();
        const int format = VIDEO_PIXFMT_BGRA;
        const size_t bytes = videoFrameSize(format, width, height);

        targetData_.resize(bytes);
        auto data = targetData_.data();

        dst.setFromMemory(data, format, width, height);
        scaler.scale(*f, dst);
        target_(data);
    }
}

void
SinkClient::registerTarget(std::function<void(const unsigned char*)>& cb)
{
    target_ = cb;
}

}} // namespace ring::video
