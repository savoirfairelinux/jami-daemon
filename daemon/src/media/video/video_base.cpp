/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "video_base.h"
#include "media_buffer.h"
#include "logger.h"

#include <cassert>

namespace ring { namespace video {

/*=== VideoPacket  ===========================================================*/

VideoPacket::VideoPacket() : packet_(static_cast<AVPacket *>(av_mallocz(sizeof(AVPacket))))
{
    av_init_packet(packet_);
}

VideoPacket::~VideoPacket() { av_free_packet(packet_); av_free(packet_); }

/*=== VideoGenerator =========================================================*/

VideoFrame&
VideoGenerator::getNewFrame()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (writableFrame_)
        writableFrame_->reset();
    else
        writableFrame_.reset(new VideoFrame());
    return *writableFrame_.get();
}

void
VideoGenerator::publishFrame()
{
    std::lock_guard<std::mutex> lk(mutex_);
    lastFrame_ = std::move(writableFrame_);
    notify(lastFrame_);
}

void
VideoGenerator::flushFrames()
{
    std::lock_guard<std::mutex> lk(mutex_);
    writableFrame_.reset();
    lastFrame_.reset();
}

std::shared_ptr<VideoFrame>
VideoGenerator::obtainLastFrame()
{
    std::lock_guard<std::mutex> lk(mutex_);
    return lastFrame_;
}

}} // namespace ring::video
