/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
#include "tone.h"
#include "logger.h"
#include "ring_types.h"
#include "string_utils.h"

#include <vector>
#include <cmath>
#include <cstdlib>

namespace jami {

Tone::Tone(std::string_view definition, unsigned int sampleRate, AVSampleFormat sampleFormat)
    : AudioLoop(AudioFormat(sampleRate, 1, sampleFormat))
{
    genBuffer(definition); // allocate memory with definition parameter
}

struct ParsedDefinition {
    unsigned total_samples;
    std::vector<std::tuple<unsigned, unsigned, unsigned>> frequencies;
};

ParsedDefinition
parseDefinition(std::string_view definition, unsigned sampleRate)
{
    ParsedDefinition parsed;
    parsed.total_samples = 0;

    std::string_view s; // portion of frequencyq
    while (getline_full(definition, s, ',')) {
        // Sample string: "350+440" or "350+440/2000,244+655/2000"
        unsigned low, high, time;
        size_t count;  // number of int for one sequence

        // The 1st frequency is before the first + or the /
        size_t pos_plus = s.find('+');
        size_t pos_slash = s.find('/');
        size_t len = s.length();
        size_t endfrequency = 0;

        if (pos_slash == std::string::npos) {
            time = 0;
            endfrequency = len;
        } else {
            time = to_int<unsigned>(s.substr(pos_slash + 1, len - pos_slash - 1), 0);
            endfrequency = pos_slash;
        }

        // without a plus = 1 frequency
        if (pos_plus == std::string::npos) {
            low = to_int<unsigned>(s.substr(0, endfrequency), 0);
            high = 0;
        } else {
            low = to_int<unsigned>(s.substr(0, pos_plus), 0);
            high = to_int<unsigned>(s.substr(pos_plus + 1, endfrequency - pos_plus - 1), 0);
        }

        // If there is time or if it's unlimited
        if (time == 0)
            count = sampleRate;
        else
            count = (sampleRate * time) / 1000;

        parsed.frequencies.emplace_back(low, high, count);
        parsed.total_samples += count;
    }
    return parsed;
}

void
Tone::genBuffer(std::string_view definition)
{
    if (definition.empty())
        return;

    auto [total_samples, frequencies] = parseDefinition(definition, buffer_->sample_rate);

    buffer_->nb_samples = total_samples;
    buffer_->format = format_.sampleFormat;
    buffer_->sample_rate = format_.sample_rate;
    av_channel_layout_default(&buffer_->ch_layout, format_.nb_channels);
    av_frame_get_buffer(buffer_.get(), 0);

    size_t outPos = 0;
    for (auto& [low, high, count] : frequencies) {
        genSin(buffer_.get(), outPos, low, high);
        outPos += count;
    }
}

void
Tone::genSin(AVFrame* buffer, unsigned outPos, unsigned lowFrequency, unsigned highFrequency)
{
    static constexpr auto PI_2 = 3.141592653589793238462643383279502884L * 2.0L;
    const double sr = (double) buffer->sample_rate;
    const double dx_h = sr ? PI_2 * lowFrequency / sr : 0.0;
    const double dx_l = sr ? PI_2 * highFrequency / sr : 0.0;
    static constexpr double DATA_AMPLITUDE_S16 = 2048;
    static constexpr double DATA_AMPLITUDE_FLT = 0.0625;
    size_t nb = buffer->nb_samples;

    if (buffer->format == AV_SAMPLE_FMT_S16 || buffer->format == AV_SAMPLE_FMT_S16P) {
        int16_t* ptr = ((int16_t*) buffer->data[0]) + outPos;
        for (size_t t = 0; t < nb; t++) {
            ptr[t] = DATA_AMPLITUDE_S16 * (sin(t * dx_h) + sin(t * dx_l));
        }
    } else if (buffer->format == AV_SAMPLE_FMT_FLT || buffer->format == AV_SAMPLE_FMT_FLTP) {
        float* ptr = ((float*) buffer->data[0]) + outPos;
        for (size_t t = 0; t < nb; t++) {
            ptr[t] = (sin(t * dx_h) + sin(t * dx_l)) * DATA_AMPLITUDE_FLT;
        }
    } else {
        JAMI_ERROR("Unsupported sample format: {}", av_get_sample_fmt_name((AVSampleFormat)buffer->format));
    }
}

} // namespace jami
