/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "recordable.h"
#include "audio/audiorecord.h"
#include "audio/ringbufferpool.h"
#include "manager.h"
#include "logger.h"

namespace ring {

Recordable::Recordable()
{}

Recordable::~Recordable()
{}

void
Recordable::initRecFilename(const std::string& /*filename*/)
{}

std::string
Recordable::getFilename() const
{
    if (recorder_)
        return recorder_->getFilename();
    else
        return "";
}

bool
Recordable::toggleRecording()
{
    std::lock_guard<std::mutex> lk {apiMutex_};
    if (!recording_ || !recorder_) {
        recorder_.reset();
        recorder_ = std::make_shared<MediaRecorder>();
        recorder_->audioOnly(isAudioOnly_);
        recorder_->setRecordingPath(Manager::instance().audioPreference.getRecordPath());
    }
    recording_ = recorder_->toggleRecording();
    return recording_;
}

void
Recordable::stopRecording()
{
    std::lock_guard<std::mutex> lk {apiMutex_};
    if (not recording_)
        return;
    if (recorder_)
        recorder_->stopRecording();
    recording_ = false;
    recorder_.reset();
}

bool
Recordable::isAudioOnly() const
{
    return isAudioOnly_;
}

} // namespace ring
