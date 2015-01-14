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

#include "shm_src.h"
#include "../shm_header.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cassert>

SHMSrc::SHMSrc(const std::string &shm_name) :
    shm_name_(shm_name),
    fd_(-1),
    shm_area_(static_cast<SHMHeader*>(MAP_FAILED)),
    shm_area_len_(0),
    buffer_gen_(0)
    {}

bool
SHMSrc::start()
{
    if (fd_ != -1) {
        std::cerr << "fd must be -1" << std::endl;
        return false;
    }

    fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
    if (fd_ < 0) {
        std::cerr << "could not open shm area \"" << shm_name_ << "\", shm_open failed" << std::endl;
        perror(strerror(errno));
        return false;
    }
    shm_area_len_ = sizeof(SHMHeader);

    shm_area_ = static_cast<SHMHeader*>(mmap(NULL, shm_area_len_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));

    if (shm_area_ == MAP_FAILED) {
        std::cerr << "Could not map shm area, mmap failed" << std::endl;
        return false;
    }

    return true;
}

bool
SHMSrc::stop()
{
    if (fd_ >= 0)
        close(fd_);
    fd_ = -1;

    if (shm_area_ != MAP_FAILED)
        munmap(shm_area_, shm_area_len_);
    shm_area_len_ = 0;
    shm_area_ = static_cast<SHMHeader*>(MAP_FAILED);

    return true;
}

bool
SHMSrc::resize_area()
{
    while ((sizeof(SHMHeader) + shm_area_->buffer_size) > shm_area_len_) {
        size_t new_size = sizeof(SHMHeader) + shm_area_->buffer_size;

        shm_unlock();
        if (munmap(shm_area_, shm_area_len_)) {
            std::cerr << "Could not unmap shared area" << std::endl;
            perror(strerror(errno));
            return false;
        }

        shm_area_ = static_cast<SHMHeader*>(mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
        shm_area_len_ = new_size;

        if (!shm_area_) {
            shm_area_ = 0;
            std::cerr << "Could not remap shared area" << std::endl;
            return false;
        }

        shm_area_len_ = new_size;
        shm_lock();
    }
    return true;
}

void SHMSrc::render(char *dest, size_t len)
{
    shm_lock();

    while (buffer_gen_ == shm_area_->buffer_gen) {
        shm_unlock();
        std::cerr << "Waiting for next buffer" << std::endl;;
        sem_wait(&shm_area_->notification);

        shm_lock();
    }

    if (!resize_area())
        return;

    std::cerr << "Reading from buffer!" << std::endl;
    memcpy(dest, shm_area_->data, len);
    buffer_gen_ = shm_area_->buffer_gen;
    shm_unlock();
}

void SHMSrc::shm_lock()
{
    sem_wait(&shm_area_->mutex);
}

void SHMSrc::shm_unlock()
{
    sem_post(&shm_area_->mutex);
}
