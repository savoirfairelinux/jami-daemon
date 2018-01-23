/*  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 *  Inspired by ringbuffer of Audacity Project
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
#include <fstream>
#include <cmath>
#include <samplerate.h>
#include <cstring>
#include <vector>
#include <climits>

#ifndef RING_UWP
#include <sndfile.hh>
#endif

#include "audiofile.h"
#include "audio/resampler.h"
#include "manager.h"
#include "client/ring_signal.h"

#include "logger.h"

namespace ring {

void
AudioFile::onBufferFinish()
{
    // We want to send values in milisecond
    const int divisor = buffer_->getSampleRate() / 1000;

    if (divisor == 0) {
        RING_ERR("Error cannot update playback slider, sampling rate is 0");
        return;
    }

    if ((updatePlaybackScale_ % 5) == 0)
        emitSignal<DRing::CallSignal::UpdatePlaybackScale>(filepath_,
                                                           (unsigned)(pos_ / divisor),
                                                           (unsigned)(buffer_->frames() / divisor));

    updatePlaybackScale_++;
}

AudioFile::AudioFile(const std::string &fileName, unsigned int sampleRate) :
    AudioLoop(sampleRate), filepath_(fileName), updatePlaybackScale_(0)
{
#ifndef RING_UWP
    int format;
    bool hasHeader = true;

    if (filepath_.find(".wav") != std::string::npos) {
        format = SF_FORMAT_WAV;
    } else if (filepath_.find(".ul") != std::string::npos) {
        format = SF_FORMAT_RAW | SF_FORMAT_ULAW;
        hasHeader = false;
    } else if (filepath_.find(".al") != std::string::npos) {
        format = SF_FORMAT_RAW | SF_FORMAT_ALAW;
        hasHeader = false;
    } else if (filepath_.find(".au") != std::string::npos) {
        format = SF_FORMAT_AU;
    } else if (filepath_.find(".flac") != std::string::npos) {
        format = SF_FORMAT_FLAC;
    } else if (filepath_.find(".ogg") != std::string::npos) {
        format = SF_FORMAT_OGG;
    } else {
        RING_WARN("No file extension, guessing WAV");
        format = SF_FORMAT_WAV;
    }

    SndfileHandle fileHandle(fileName.c_str(), SFM_READ, format, hasHeader ? 0 : 1,
                             hasHeader ? 0 : 8000);

    if (!fileHandle)
        throw AudioFileException("File handle " + fileName + " could not be created");
    if (fileHandle.error()) {
        RING_ERR("Error fileHandle: %s", fileHandle.strError());
        throw AudioFileException("File " + fileName + " doesn't exist");
    }

    switch (fileHandle.channels()) {
        case 1:
        case 2:
            break;
        default:
            throw AudioFileException("Unsupported number of channels");
    }

    // get # of bytes in file
    const size_t fileSize = fileHandle.seek(0, SEEK_END);
    fileHandle.seek(0, SEEK_SET);

    const sf_count_t nbFrames = hasHeader ? fileHandle.frames() : fileSize / fileHandle.channels();

    AudioSample * interleaved = new AudioSample[nbFrames * fileHandle.channels()];

    // get n "items", aka samples (not frames)
    fileHandle.read(interleaved, nbFrames * fileHandle.channels());

    AudioBuffer * buffer = new AudioBuffer(nbFrames, AudioFormat(fileHandle.samplerate(), fileHandle.channels()));
    buffer->deinterleave(interleaved, nbFrames, fileHandle.channels());
    delete [] interleaved;

    const int rate = static_cast<int32_t>(sampleRate);

    if (fileHandle.samplerate() != rate) {
        Resampler resampler(std::max(fileHandle.samplerate(), rate), fileHandle.channels(), true);
        AudioBuffer * resampled = new AudioBuffer(nbFrames, AudioFormat(rate, fileHandle.channels()));
        resampler.resample(*buffer, *resampled);
        delete buffer;
        delete buffer_;
        buffer_ = resampled;
    } else {
        delete buffer_;
        buffer_ = buffer;
    }
#endif
}

} // namespace ring
