/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

#include "localrecorder.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "client/videomanager.h"
#include "media_stream.h"
#include "manager.h"
#include "logger.h"

namespace ring {

LocalRecorder::LocalRecorder(const bool& audioOnly) {
    isAudioOnly_ = audioOnly;
    recorder_->audioOnly(audioOnly);
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
    // create read offset in RingBuffer
    Manager::instance().getRingBufferPool().bindHalfDuplexOut(path_, RingBufferPool::DEFAULT_ID);
    Manager::instance().startAudioDriverStream();

    audioInput_.reset(new AudioInput(path_));
    audioInput_->setFormat(AudioFormat::STEREO());
    audioInput_->initRecorder(recorder_);

#ifdef RING_VIDEO
    // video recording
    if (!isAudioOnly_) {
        videoInput_ = std::static_pointer_cast<video::VideoInput>(ring::getVideoCamera());
        if (videoInput_) {
            videoInput_->initRecorder(recorder_);
        } else {
            RING_ERR() << "Unable to record video (no video input)";
            return false;
        }
    }
#endif

    return Recordable::startRecording(path_);
}

void
LocalRecorder::stopRecording()
{
    Recordable::stopRecording();
    Manager::instance().getRingBufferPool().unBindHalfDuplexOut(path_, RingBufferPool::DEFAULT_ID);
    audioInput_.reset();
    if (videoInput_)
        videoInput_->initRecorder(nullptr); // workaround for deiniting recorder
    videoInput_.reset();
}

} // namespace ring
