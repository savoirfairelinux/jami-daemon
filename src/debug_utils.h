/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#include "config.h"

#include "libav_deps.h"

#include <chrono>
#include <fstream>
#include <ios>
#include <ratio>

#warning Debug utilities included in build

using Clock = std::chrono::steady_clock;

namespace ring { namespace debug {

/**
 * Ex:
 * Timer t;
 * std::this_thread::sleep_for(std::chrono::milliseconds(10));
 * RING_DBG() << "Task took " << t.getDuration<std::chrono::nanoseconds>() << " ns";
 */
class Timer
{
public:
    Timer() { start_ = Clock::now(); }

    template<class Period = std::ratio<1>>
    uint64_t getDuration()
    {
        auto diff = std::chrono::duration_cast<Period>(Clock::now() - start_);
        return diff.count();
    }

private:
    std::chrono::time_point<Clock> start_;
};

class WavWriter
{
public:
    WavWriter(std::string filename, int channels, int sampleRate, int bytesPerSample)
    {
        union {
            uint32_t i;
            char c[4];
        } big = {0x01020304};
        bigEndian_ = (big.c[0] == 1);

        f_ = std::ofstream(filename, std::ios::binary);
        f_ << "RIFF----WAVEfmt ";
        write(16, 4); // no extension data
        write(1, 2); // PCM integer samples
        write(channels, 2); // channels
        write(sampleRate, 4); // sample rate
        write(sampleRate * channels * bytesPerSample, 4); // sample size
        write(4, 2); // data block size
        write(bytesPerSample * 8, 2); // bits per sample
        dataChunk_ = f_.tellp();
        f_ << "data----";
    }

    ~WavWriter()
    {
        length_ = f_.tellp();
        f_.seekp(dataChunk_ + 4);
        write(length_ - dataChunk_ + 8, 4);
        f_.seekp(4);
        write(length_ - 8, 4);
    }

    template<typename Word>
    void write(Word value, unsigned size)
    {
        if (bigEndian_) {
            while (size)
                f_.put(static_cast<char>((value >> (8 * --size)) & 0xFF));
        } else {
            for (; size; --size, value >>= 8)
                f_.put(static_cast<char>(value & 0xFF));
        }
    }

    void write(AVFrame* frame)
    {
        AVSampleFormat fmt = (AVSampleFormat)frame->format;
        int channels = frame->channels;
        int depth = av_get_bytes_per_sample(fmt);
        int linesize = frame->linesize[0];
        int planar = av_sample_fmt_is_planar(fmt);
        for (int ch = 0; ch < channels; ++ch) {
            if (planar) {
                for (int i = 0; i < linesize; i += depth) {
                    writeSample(&frame->extended_data[ch][i], fmt, depth);
                }
            } else {
                for (int i = 0; i < linesize; i += depth * channels) {
                    writeSample(&frame->extended_data[0][i + depth * ch], fmt, depth);
                }
            }
        }
    }

private:
    void writeSample(uint8_t* p, AVSampleFormat format, int size)
    {
        switch (format) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            write(*(uint8_t*)p, size);
            break;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            write(*(int16_t*)p, size);
            break;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            write(*(int32_t*)p, size);
            break;
        case AV_SAMPLE_FMT_S64:
        case AV_SAMPLE_FMT_S64P:
            write(*(int64_t*)p, size);
            break;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            // float samples are always 32 bits in FFmpeg
            write(*(int32_t*)p, size);
            break;
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            // dbl samples are always 64 bits in FFmpeg
            write(*(int64_t*)p, size);
            break;
        default:
            break;
        }
    }

    std::ofstream f_;
    size_t dataChunk_;
    size_t length_;
    bool bigEndian_;
};

}} // namespace ring::debug
