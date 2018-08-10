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


#include "audio_input.h"
#include "media_encoder.h"

namespace ring { namespace audio {

AudioInput::AudioInput() {}

AudioInput::~AudioInput()
{
    if (auto rec = recorder_.lock())
        rec->stopRecording();
}

void
AudioInput::process()
{
    // TODO
}

bool
AudioInput::isCapturing() const noexcept
{
    // TODO
}

bool AudioInput::captureFrame()
{
    // TODO
}

void
AudioInput::createEncoder()
{
    deleteEncoder();
    // TODO
}

void
AudioInput::deleteEncoder()
{
    if (not encoder_)
        return;
    encoder_.reset();
}

bool
AudioInput::initFile(std::string path)
{
    // TODO
}

std::shared_future<DeviceParams>
AudioInput::switchInput(const std::string& resource)
{
    // TODO
}

void
AudioInput::initRecorder(const std::shared_ptr<MediaRecorder>& rec)
{
    rec->incrementStreams(1);
    recorder_ = rec;
}

}} // namespace ring::audio
