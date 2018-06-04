/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

#include "localrecorder.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "media_stream.h"
#include "manager.h"
#include "logger.h"

namespace ring {

LocalRecorder::LocalRecorder(std::shared_ptr<ring::video::VideoInput> input) {
    if (input) {
        videoInput_ = input;
        videoInputSet_ = true;
    } else {
        isAudioOnly_ = true;
    }
}

void
LocalRecorder::setPath(const std::string& path)
{
    if (isRecording()) {
        RING_ERR("can't set path while recording");
        return;
    }

    recorder_->setPath(path);
    path_ = path;
}

bool
LocalRecorder::startRecording()
{
    if (isRecording()) {
        RING_ERR("recording already started!");
        return false;
    }

    if (path_.empty()) {
        RING_ERR("could not start recording (path not set)");
        return false;
    }

    if (!recorder_) {
        RING_ERR("could not start recording (no recorder)");
        return false;
    }

    // audio recording
    audioInput_.reset(new ring::audio::AudioInput(RingBufferPool::DEFAULT_ID));
    auto rb = ring::Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID);
    rb->createReadOffset(RingBufferPool::DEFAULT_ID);
    ring::Manager::instance().getAudioDriver()->flushUrgent();
    ring::Manager::instance().getAudioDriver()->flushMain();
    ring::Manager::instance().startAudioDriverStream();
    audioInput_->initRecorder(recorder_);

#ifdef RING_VIDEO
    // video recording
    if (!isAudioOnly_) {
        if (videoInputSet_) {
            auto videoInputShpnt = videoInput_.lock();
            videoInputShpnt->initRecorder(recorder_);
        } else {
            RING_ERR("[BUG] can't record video (video input pointer is null)");
            return false;
        }
    }
#endif

    return Recordable::startRecording(path_);
}

void
LocalRecorder::stopRecording()
{
    if (audioInput_) {
        auto rb = ring::Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID);
        rb->removeReadOffset(RingBufferPool::DEFAULT_ID);
        audioInput_.reset();
        audioInput_ = nullptr;
    } else {
        RING_ERR("could not stop audio layer (audio input is null)");
    }

    Recordable::stopRecording();
}

} // namespace ring
