/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
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
#include "client/videomanager.h"

namespace jami {

LocalRecorder::LocalRecorder(const bool& audioOnly)
{
    isAudioOnly_ = audioOnly;
    recorder_->audioOnly(audioOnly);
}

LocalRecorder::~LocalRecorder()
{
    if (isRecording())
        stopRecording();
}

void
LocalRecorder::setPath(const std::string& path)
{
    if (isRecording()) {
        JAMI_ERR("can't set path while recording");
        return;
    }

    recorder_->setPath(path);
    path_ = path;
}

bool
LocalRecorder::startRecording()
{
    if (isRecording()) {
        JAMI_ERR("recording already started!");
        return false;
    }

    if (path_.empty()) {
        JAMI_ERR("could not start recording (path not set)");
        return false;
    }

    if (!recorder_) {
        JAMI_ERR("could not start recording (no recorder)");
        return false;
    }

    // audio recording
    // create read offset in RingBuffer
    Manager::instance().getRingBufferPool().bindHalfDuplexOut(path_, RingBufferPool::DEFAULT_ID);
    Manager::instance().startAudioDriverStream();

    audioInput_ = jami::getAudioInput(path_);
    audioInput_->setFormat(AudioFormat::STEREO());
    audioInput_->attach(recorder_->addStream(audioInput_->getInfo()));

#ifdef ENABLE_VIDEO
    // video recording
    if (!isAudioOnly_) {
        videoInput_ = std::static_pointer_cast<video::VideoInput>(jami::getVideoCamera());
        if (videoInput_) {
            videoInput_->attach(recorder_->addStream(videoInput_->getInfo()));
        } else {
            JAMI_ERR() << "Unable to record video (no video input)";
            return false;
        }
    }
#endif

    return Recordable::startRecording(path_);
}

void
LocalRecorder::stopRecording()
{
    if (auto ob = recorder_->getStream(audioInput_->getInfo().name))
        audioInput_->detach(ob);
    if (videoInput_)
        if (auto ob = recorder_->getStream(videoInput_->getInfo().name))
            videoInput_->detach(ob);
    Manager::instance().getRingBufferPool().unBindHalfDuplexOut(path_, RingBufferPool::DEFAULT_ID);
    // NOTE stopRecording should be last call to avoid data races
    Recordable::stopRecording();
}

} // namespace jami
