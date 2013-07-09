/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#include "audioloop.h"
#include "manager.h"
#include "client/callmanager.h"
#include <cmath>
#include <numeric>
#include <cstring>
#include <cassert>
#include "logger.h"

AudioLoop::AudioLoop(unsigned int sampleRate) : buffer_(), pos_(0), isRecording_(false)
{
    buffer_ = new AudioBuffer();
    buffer_->setSampleRate(sampleRate);
}

AudioLoop::~AudioLoop()
{
    delete buffer_;
}

void
AudioLoop::seek(double relative_position)
{
    size_t new_pos = (size_t)((double)buffer_->samples() * (relative_position * 0.01));

    pos_ = new_pos;
}

static unsigned int updatePlaybackScale = 0;

void
AudioLoop::getNext(AudioBuffer& output, unsigned int volume)
{
    if(!buffer_) {
        ERROR("AudioLoop::buffer_ is not set (NULL pointer)");
        return;
    }

    const size_t buf_samples = buffer_->samples();
    size_t pos = pos_;
    size_t total_samples = output.samples();
    size_t output_pos = 0;

    if (buf_samples == 0) {
        ERROR("Audio loop size is 0");
        return;
    } else if (pos >= buf_samples) {
        ERROR("Invalid loop position %d", pos);
        return;
    }

    while (total_samples > 0) {
        size_t samples = std::min(total_samples, buf_samples - pos);

        output.copy(*buffer_, samples, pos, output_pos);

        output_pos += samples;
        pos = (pos + samples) % buf_samples;

        total_samples -= samples;
    }

    output.applyGain(volume); // apply volume

    pos_ = pos;

    // We want to send values in milisecond
    int divisor = buffer_->getSampleRate() / 1000;
    if (divisor == 0) {
        ERROR("Error cannot update playback slider, sampling rate is 0");
        return;
    }

    if (isRecording_) {
        if ((updatePlaybackScale % 5) == 0) {
            CallManager *cm = Manager::instance().getClient()->getCallManager();
            cm->updatePlaybackScale(pos_ / divisor, buf_samples / divisor);
        }

        updatePlaybackScale++;
    }
}

