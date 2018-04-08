/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
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

#include "libav_deps.h"
#include "media_recorder.h"

namespace ring {

MediaRecorder::MediaRecorder()
{}

MediaRecorder::~MediaRecorder()
{}

void
MediaRecorder::initFilename(const std::string &peerNumber)
{
}

std::string
MediaRecorder::getFilename() const
{ return filename_; }

bool
MediaRecorder::fileExists() const
{
    return false;
}

bool
MediaRecorder::isRecording() const
{ return isRecording_; }

bool
MediaRecorder::toggleRecording()
{
    isRecording_ = !isRecording_;
}

void
MediaRecorder::stopRecording()
{
    flush();
    isRecording_ = false;
}

void
MediaRecorder::recordData(AVPacket* packet)
{
}

void
MediaRecorder::flush()
{
}

} // namespace ring
