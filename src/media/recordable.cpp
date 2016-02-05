/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
#include "audio/audiorecorder.h"
#include "manager.h"
#include "logger.h"

namespace ring {

Recordable::Recordable()
    : recAudio_(new AudioRecord)
    , recorder_(new AudioRecorder {recAudio_.get(), Manager::instance().getRingBufferPool()})
{
    auto record_path = Manager::instance().audioPreference.getRecordPath();
    RING_DBG("Set recording options: %s", record_path.c_str());
    recAudio_->setRecordingOptions(AudioFormat::MONO(), record_path);
}

Recordable::~Recordable()
{
    if (recAudio_->isOpenFile())
        recAudio_->closeFile();
}

void
Recordable::initRecFilename(const std::string &filename)
{
    recAudio_->initFilename(filename);
}

std::string
Recordable::getFilename() const
{
    return recAudio_->getFilename();
}

void
Recordable::setRecordingFormat(AudioFormat format)
{
    recAudio_->setSndFormat(format);
}

bool
Recordable::isRecording() const
{
    return recAudio_->isRecording();
}

bool
Recordable::toggleRecording()
{
    if (not isRecording())
        recorder_->init();
    return recAudio_->toggleRecording();
}

void
Recordable::stopRecording()
{
    recAudio_->stopRecording();
}

} // namespace ring
