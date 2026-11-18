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
#include <cstring>

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
    copyToneSamples(frame);
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

    copyToneSamples(frame);
}

void
DTMFGenerator::copyToneSamples(AVFrame* frame)
{
    if (!frame || !frame->data[0] || frame->nb_samples <= 0)
        throw DTMFException("Invalid output frame");

    const AVFrame* tone = state.sample;
    if (!tone || !tone->data[0] || tone->nb_samples <= 0)
        throw DTMFException("DTMF tone buffer not available");

    const auto outFormat = (AVSampleFormat) frame->format;
    if (av_get_packed_sample_fmt(outFormat) != av_get_packed_sample_fmt((AVSampleFormat) tone->format))
        throw DTMFException("Unsupported output sample format");

    const int channels = frame->ch_layout.nb_channels;
    if (channels <= 0)
        throw DTMFException("Invalid output channel count");

    const bool planar = av_sample_fmt_is_planar(outFormat);
    if (planar) {
        if (!frame->extended_data)
            throw DTMFException("Invalid output frame");
        for (int ch = 0; ch < channels; ++ch) {
            if (!frame->extended_data[ch])
                throw DTMFException("Invalid output frame");
        }
    }

    const auto bps = (size_t) av_get_bytes_per_sample(outFormat);
    const auto toneSamples = (unsigned) tone->nb_samples;
    const uint8_t* src = tone->data[0];
    if (state.offset >= toneSamples)
        state.offset = 0;

    // The tone buffer is mono: loop over it and duplicate it on every output channel.
    size_t outPos = 0;
    auto remaining = (unsigned) frame->nb_samples;
    while (remaining > 0) {
        const unsigned count = std::min(remaining, toneSamples - state.offset);
        const uint8_t* in = src + state.offset * bps;
        if (planar) {
            for (int ch = 0; ch < channels; ++ch)
                std::memcpy(frame->extended_data[ch] + outPos * bps, in, count * bps);
        } else {
            uint8_t* out = frame->data[0] + outPos * channels * bps;
            for (size_t i = 0; i < count; ++i)
                for (int ch = 0; ch < channels; ++ch)
                    std::memcpy(out + (i * channels + ch) * bps, in + i * bps, bps);
        }
        outPos += count;
        remaining -= count;
        state.offset = (state.offset + count) % toneSamples;
    }
}

AudioFormat
DTMFGenerator::getFormat() const
{
    return AudioFormat(sampleRate_, 1, tone_.getFormat().sampleFormat);
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
