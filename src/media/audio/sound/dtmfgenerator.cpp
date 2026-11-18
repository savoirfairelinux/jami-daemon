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

namespace {

double
readSample(const AVFrame* src, size_t index)
{
    switch (src->format) {
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
        return reinterpret_cast<const int16_t*>(src->data[0])[index] / 32768.0;
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
        return reinterpret_cast<const float*>(src->data[0])[index];
    default:
        return 0.0;
    }
}

int16_t
toS16(double value)
{
    return static_cast<int16_t>(std::clamp(value * 32768.0, -32768.0, 32767.0));
}

bool
writeSample(AVFrame* dst, int channel, int index, double value)
{
    const int nbChannels = dst->ch_layout.nb_channels;
    switch (dst->format) {
    case AV_SAMPLE_FMT_S16:
        reinterpret_cast<int16_t*>(dst->extended_data[0])[index * nbChannels + channel] = toS16(value);
        return true;
    case AV_SAMPLE_FMT_S16P:
        reinterpret_cast<int16_t*>(dst->extended_data[channel])[index] = toS16(value);
        return true;
    case AV_SAMPLE_FMT_FLT:
        reinterpret_cast<float*>(dst->extended_data[0])[index * nbChannels + channel] = static_cast<float>(value);
        return true;
    case AV_SAMPLE_FMT_FLTP:
        reinterpret_cast<float*>(dst->extended_data[channel])[index] = static_cast<float>(value);
        return true;
    default:
        return false;
    }
}

} // namespace

/*
 * Copy samples from the current (mono) tone buffer into the output frame,
 * wrapping around the tone buffer and duplicating the tone on every output
 * channel. The output frame may differ from the tone buffer in channel
 * count, sample format or length.
 */
void
DTMFGenerator::copyToneSamples(AVFrame* frame)
{
    if (!frame || frame->nb_samples <= 0)
        return;

    const int nbChannels = frame->ch_layout.nb_channels;
    const auto format = (AVSampleFormat) frame->format;
    if (!frame->extended_data || !frame->extended_data[0] || nbChannels <= 0)
        throw DTMFException("Invalid output frame");
    if (av_sample_fmt_is_planar(format)) {
        for (int ch = 0; ch < nbChannels; ++ch)
            if (!frame->extended_data[ch])
                throw DTMFException("Invalid output frame");
    }

    const AVFrame* src = state.sample;
    if (!src || !src->data[0] || src->nb_samples <= 0) {
        av_samples_set_silence(frame->extended_data, 0, frame->nb_samples, nbChannels, format);
        return;
    }

    const auto srcLen = static_cast<size_t>(src->nb_samples);
    size_t pos = state.offset % srcLen;
    for (int i = 0; i < frame->nb_samples; ++i) {
        const double value = readSample(src, pos);
        for (int ch = 0; ch < nbChannels; ++ch) {
            if (!writeSample(frame, ch, i, value)) {
                av_samples_set_silence(frame->extended_data, 0, frame->nb_samples, nbChannels, format);
                return;
            }
        }
        if (++pos == srcLen)
            pos = 0;
    }
    state.offset = static_cast<unsigned>(pos);
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
    if (av_frame_get_buffer(ptr.get(), 0) < 0) {
        ptr->nb_samples = 0;
        return ptr;
    }
    tone_.genSin(ptr.get(), 0, ptr->nb_samples, TONES[index].higher, TONES[index].lower);
    return ptr;
}

} // namespace jami
