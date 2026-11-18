/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "dtmfgenerator.h"
#include "libav_deps.h"

#include <algorithm>
#include <cmath>
#include <cassert>

namespace jami {

/** Struct to handle a DTMF */
struct DTMFTone
{
    unsigned char code; /** Code of the tone */
    unsigned lower;     /** Lower frequency */
    unsigned higher;    /** Higher frequency */
};

/*
 * Tone frequencies
 */
constexpr DTMFTone TONES[] = {{'0', 941, 1336},
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

DTMFGenerator::~DTMFGenerator() {}

void
DTMFGenerator::getSamples(AVFrame* frame, unsigned char code)
{
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

    state.offset = 0;
    copyTone(frame);
}

/*
 * Get next n samples (continues where previous call to
 * genSample or genNextSamples stopped
 */
void
DTMFGenerator::getNextSamples(AVFrame* frame)
{
    if (state.sample == 0)
        throw DTMFException("DTMF generator not initialized");

    copyTone(frame);
}

/*
 * Copy the current tone into frame, looping over the tone buffer.
 * The frame must use the tone format; callers convert it afterwards.
 */
void
DTMFGenerator::copyTone(AVFrame* frame)
{
    const AVFrame* tone = state.sample;
    if (frame->format != tone->format || frame->sample_rate != tone->sample_rate
        || av_channel_layout_compare(&frame->ch_layout, &tone->ch_layout) != 0)
        throw DTMFException("Frame format does not match DTMF format");

    for (int pos = 0; pos < frame->nb_samples;) {
        int count = std::min(frame->nb_samples - pos, tone->nb_samples - static_cast<int>(state.offset));
        av_samples_copy(frame->data,
                        tone->data,
                        pos,
                        state.offset,
                        count,
                        tone->ch_layout.nb_channels,
                        static_cast<AVSampleFormat>(tone->format));
        pos += count;
        state.offset = (state.offset + count) % tone->nb_samples;
    }
}

libjami::FrameBuffer
DTMFGenerator::fillToneBuffer(unsigned index)
{
    assert(index < NUM_TONES);
    libjami::FrameBuffer ptr(av_frame_alloc());
    ptr->nb_samples = sampleRate_;
    ptr->format = tone_.getFormat().sampleFormat;
    ptr->sample_rate = sampleRate_;
    av_channel_layout_from_mask(&ptr->ch_layout, AV_CH_LAYOUT_MONO);
    av_frame_get_buffer(ptr.get(), 0);
    tone_.genSin(ptr.get(), 0, ptr->nb_samples, TONES[index].higher, TONES[index].lower);
    return ptr;
}

} // namespace jami
