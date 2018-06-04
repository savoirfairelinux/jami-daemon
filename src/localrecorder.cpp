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

namespace ring {

LocalRecorder::LocalRecorder(std::shared_ptr<ring::video::VideoInput> input) {
    if (!input) {
        throw std::invalid_argument("passed VideoInput pointer is NULL");
    }

    videoInput_ = input;
}

bool
LocalRecorder::startRecording()
{
    const bool startRecording = Recordable::toggleRecording();
    if (startRecording) {
#ifdef RING_VIDEO
        if (!isAudioOnly_ && recorder_) {
            auto videoInput_shpnt = videoInput_.lock();
            videoInput_shpnt->initRecorder(recorder_);
        }
#endif
    }
    return startRecording;
}
}
