/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Beraud <adrien.beraud@wisdomvibes.com>
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

#pragma once

extern "C" {
#include <libavutil/samplefmt.h>
}

#include <fmt/core.h>
#include <sstream>
#include <string>
#include <cstddef> // for size_t

namespace jami {

/**
 * Structure to hold sample rate and channel number associated with audio data.
 */
struct AudioFormat
{
    unsigned sample_rate;
    unsigned nb_channels;
    AVSampleFormat sampleFormat;

    constexpr AudioFormat(unsigned sr, unsigned c, AVSampleFormat f = AV_SAMPLE_FMT_S16)
        : sample_rate(sr)
        , nb_channels(c)
        , sampleFormat(f)
    {}

    inline bool operator==(const AudioFormat& b) const
    {
        return ((b.sample_rate == sample_rate) && (b.nb_channels == nb_channels)
                && (b.sampleFormat == sampleFormat));
    }

    inline bool operator!=(const AudioFormat& b) const { return !(*this == b); }

    inline std::string toString() const
    {
        return fmt::format("{{{}, {} channels, {}Hz}}", av_get_sample_fmt_name(sampleFormat), nb_channels, sample_rate);
    }

    /**
     * Returns bytes necessary to hold one frame of audio data.
     */
    inline size_t getBytesPerFrame() const { return av_get_bytes_per_sample(sampleFormat) * nb_channels; }

    /**
     * Bytes per second (default), or bytes necessary
     * to hold delay_ms milliseconds of audio data.
     */
    inline size_t getBandwidth(unsigned delay_ms = 1000) const
    {
        return (getBytesPerFrame() * sample_rate * delay_ms) / 1000;
    }

    static const constexpr unsigned DEFAULT_SAMPLE_RATE = 48000;
    static const constexpr AudioFormat DEFAULT() { return AudioFormat {16000, 1}; }
    static const constexpr AudioFormat NONE() { return AudioFormat {0, 0}; }
    static const constexpr AudioFormat MONO() { return AudioFormat {DEFAULT_SAMPLE_RATE, 1}; }
    static const constexpr AudioFormat STEREO() { return AudioFormat {DEFAULT_SAMPLE_RATE, 2}; }
};

} // namespace jami
