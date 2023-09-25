/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
#include "dtmfgenerator.h"
#include "libav_deps.h"

#include <cmath>
#include <cassert>
#include <ciso646> // fix windows compiler bug

namespace jami {

/*
 * Tone frequencies
 */
const DTMFGenerator::DTMFTone DTMFGenerator::tones_[] = {{'0', 941, 1336},
                                                         {'1', 697, 1209},
                                                         {'2', 697, 1336},
                                                         {'3', 697, 1477},
                                                         {'4', 770, 1209},
                                                         {'5', 770, 1336},
                                                         {'6', 770, 1477},
                                                         {'7', 852, 1209},
                                                         {'8', 852, 1336},
                                                         {'9', 852, 1477},
                                                         {'A', 697, 1633},
                                                         {'B', 770, 1633},
                                                         {'C', 852, 1633},
                                                         {'D', 941, 1633},
                                                         {'*', 941, 1209},
                                                         {'#', 941, 1477}};

/*
 * Initialize the generator
 */
DTMFGenerator::DTMFGenerator(unsigned int sampleRate, AVSampleFormat sampleFormat)
    : state()
    , sampleRate_(sampleRate)
    , tone_("", sampleRate, sampleFormat)
{
    state.offset = 0;
    state.sample = 0;

    for (int i = 0; i < NUM_TONES; i++)
        toneBuffers_[i] = fillToneBuffer(i);
}

DTMFGenerator::~DTMFGenerator()
{}

using std::vector;

void DTMFGenerator::getSamples(AVFrame* frame, unsigned char code) {
    code = toupper(code);

    if (code >= '0' and code <= '9')
        state.sample = toneBuffers_[code - '0'].get();
    else if (code >= 'A' and code <= 'D')
        state.sample = toneBuffers_[code - 'A' + 10].get();
    else {
        switch (code) {
        case '*':
            state.sample = toneBuffers_[NUM_TONES - 2].get();
            break;

        case '#':
            state.sample = toneBuffers_[NUM_TONES - 1].get();
            break;

        default:
            throw DTMFException("Invalid code");
            break;
        }
    }

    av_samples_copy(frame->data, state.sample->data, 0, state.offset, frame->nb_samples, frame->ch_layout.nb_channels, (AVSampleFormat)frame->format);
    state.offset = frame->nb_samples % sampleRate_;
}

/*
 * Get next n samples (continues where previous call to
 * genSample or genNextSamples stopped
 */
void DTMFGenerator::getNextSamples(AVFrame* frame) {
    if (state.sample == 0)
        throw DTMFException("DTMF generator not initialized");

    av_samples_copy(frame->data, state.sample->data, 0, state.offset, frame->nb_samples, frame->ch_layout.nb_channels, (AVSampleFormat)frame->format);
    state.offset = (state.offset + frame->nb_samples) % sampleRate_;
}


libjami::FrameBuffer
DTMFGenerator::fillToneBuffer(int index)
{
    assert(index >= 0 and index < NUM_TONES);
    libjami::FrameBuffer ptr(av_frame_alloc());
    ptr->nb_samples = sampleRate_;
    ptr->format = tone_.getFormat().sampleFormat;
    ptr->sample_rate = sampleRate_;
    ptr->channel_layout = AV_CH_LAYOUT_MONO;
    ptr->ch_layout = AV_CHANNEL_LAYOUT_MONO;
    av_frame_get_buffer(ptr.get(), 0);
    tone_.genSin(ptr.get(), 0, tones_[index].higher, tones_[index].lower);
    return ptr;
}

} // namespace jami
