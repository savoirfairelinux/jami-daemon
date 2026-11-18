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

#include <cmath>
#include <algorithm>
#include <cassert>
#include <cstdint>

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

namespace {

double
readMonoSample(const AVFrame* src, unsigned index)
{
    const auto fmt = av_get_packed_sample_fmt(static_cast<AVSampleFormat>(src->format));
    if (fmt == AV_SAMPLE_FMT_S16)
        return reinterpret_cast<const int16_t*>(src->data[0])[index] / 32768.0;
    if (fmt == AV_SAMPLE_FMT_FLT)
        return reinterpret_cast<const float*>(src->data[0])[index];
    return 0.0;
}

void
writeSample(AVFrame* dst, int channel, int index, double value)
{
    const auto format = static_cast<AVSampleFormat>(dst->format);
    const auto fmt = av_get_packed_sample_fmt(format);
    const bool planar = av_sample_fmt_is_planar(format) != 0;
    uint8_t* plane = planar ? dst->extended_data[channel] : dst->extended_data[0];
    if (plane == nullptr)
        return;
    const int pos = planar ? index : index * dst->ch_layout.nb_channels + channel;
    value = std::clamp(value, -1.0, 1.0);
    if (fmt == AV_SAMPLE_FMT_S16)
        reinterpret_cast<int16_t*>(plane)[pos] = static_cast<int16_t>(value * 32767.0);
    else if (fmt == AV_SAMPLE_FMT_FLT)
        reinterpret_cast<float*>(plane)[pos] = static_cast<float>(value);
}

} // namespace

/*
 * Initialize the generator
 */
DTMFGenerator::DTMFGenerator(unsigned int sampleRate, AVSampleFormat sampleFormat)
    : state()
    , sampleRate_(sampleRate)
    , tone_("", sampleRate, sampleFormat)
{
    state.offset = 0;
    state.sample = nullptr;

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
    if (state.sample == nullptr)
        throw DTMFException("DTMF generator not initialized");

    copyToneSamples(frame);
}

/*
 * Copy samples from the current (mono) tone buffer into the output frame,
 * duplicating them on every output channel and wrapping around the tone
 * buffer. The output frame may have a different channel count, sample format
 * or length than the tone buffer, so av_samples_copy cannot be used safely.
 */
void
DTMFGenerator::copyToneSamples(AVFrame* frame)
{
    if (frame == nullptr || frame->extended_data == nullptr || frame->nb_samples <= 0
        || frame->ch_layout.nb_channels <= 0 || state.sample == nullptr || state.sample->data[0] == nullptr
        || state.sample->nb_samples <= 0)
        throw DTMFException("Invalid DTMF buffer");

    const auto toneLength = static_cast<unsigned>(state.sample->nb_samples);
    const int channels = frame->ch_layout.nb_channels;
    unsigned offset = state.offset % toneLength;
    for (int i = 0; i < frame->nb_samples; ++i) {
        const double value = readMonoSample(state.sample, offset);
        for (int ch = 0; ch < channels; ++ch)
            writeSample(frame, ch, i, value);
        offset = (offset + 1) % toneLength;
    }
    state.offset = offset;
}

libjami::FrameBuffer
DTMFGenerator::fillToneBuffer(unsigned index)
{
    assert(index < NUM_TONES);
    libjami::FrameBuffer ptr(av_frame_alloc());
    if (!ptr)
        return ptr;
    ptr->nb_samples = static_cast<int>(sampleRate_);
    ptr->format = tone_.getFormat().sampleFormat;
    ptr->sample_rate = static_cast<int>(sampleRate_);
    av_channel_layout_from_mask(&ptr->ch_layout, AV_CH_LAYOUT_MONO);
    if (av_frame_get_buffer(ptr.get(), 0) < 0)
        return {};
    tone_.genSin(ptr.get(), 0, ptr->nb_samples, TONES[index].higher, TONES[index].lower);
    return ptr;
}

} // namespace jami
