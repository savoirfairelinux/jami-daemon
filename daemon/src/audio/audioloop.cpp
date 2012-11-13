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
#include "dbus/callmanager.h"
#include <cmath>
#include <numeric>
#include <cstring>
#include <cassert>
#include "logger.h"

AudioLoop::AudioLoop(unsigned int sampleRate) : buffer_(0),  size_(0), pos_(0), sampleRate_(sampleRate), isRecording_(false)
{}

AudioLoop::~AudioLoop()
{
    delete [] buffer_;
}

void
AudioLoop::seek(double relative_position)
{
    size_t new_pos = (size_t)((double)size_ * (relative_position * 0.01));

    pos_ = new_pos;
}

static unsigned int updatePlaybackScale = 0;

void
AudioLoop::getNext(SFLDataFormat* output, size_t total_samples, short volume)
{
    size_t pos = pos_;

    if (size_ == 0) {
        ERROR("Audio loop size is 0");
        return;
    } else if (pos >= size_) {
        ERROR("Invalid loop position %d", pos);
        return;
    }

    while (total_samples > 0) {
        size_t samples = total_samples;

        if (samples > (size_ - pos))
            samples = size_ - pos;

        // short->char conversion
        memcpy(output, buffer_ + pos, samples * sizeof(SFLDataFormat));

        // Scaling needed
        if (volume != 100) {
            const double gain = volume * 0.01;

            for (size_t i = 0; i < samples; ++i, ++output)
                *output *= gain;
        } else
            output += samples; // this is the destination...

        pos = (pos + samples) % size_;

        total_samples -= samples;
    }

    pos_ = pos;

    // We want to send values in milisecond
    int divisor = sampleRate_ / 1000;
    if(divisor == 0) {
        ERROR("Error cannot update playback slider, sampling rate is 0");
        return;
    }

    if(isRecording_) {
#if HAVE_DBUS
        if((updatePlaybackScale % 5) == 0) {
            CallManager *cm = Manager::instance().getDbusManager()->getCallManager();
            cm->updatePlaybackScale(pos_ / divisor, size_ / divisor);
        }
#endif
        updatePlaybackScale++;
    }
}

