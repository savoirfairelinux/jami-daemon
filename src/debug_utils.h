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

#include "audio/resampler.h"
#include "libav_deps.h"

#include <chrono>
#include <fstream>
#include <ios>
#include <ratio>
#include "logger.h"

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

/**
 * Minimally invasive audio logger. Writes a wav file from raw PCM or AVFrame. Helps debug what goes wrong with audio.
 */
class WavWriter
{
public:
    WavWriter(std::string filename, int channels, int sampleRate, int bytesPerSample)
    {
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
        for (; size; --size, value >>= 8)
            f_.put(static_cast<char>(value & 0xFF));
    }

    void write(AVFrame* frame)
    {
        resample(frame);
        AVSampleFormat fmt = (AVSampleFormat)frame->format;
        int channels = frame->channels;
        int depth = av_get_bytes_per_sample(fmt);
        int planar = av_sample_fmt_is_planar(fmt);
        int step = (planar ? depth : depth * channels);
        for (int i = 0; i < frame->nb_samples; ++i) {
            int offset = i * step;
            for (int ch = 0; ch < channels; ++ch) {
                if (planar)
                    writeSample(&frame->extended_data[ch][offset], fmt, depth);
                else
                    writeSample(&frame->extended_data[0][offset + ch * depth], fmt, depth);
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
        default:
            break;
        }
    }

    void resample(AVFrame* frame)
    {
        auto fmt = (AVSampleFormat)frame->format;
        switch (fmt) {
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
        {
            if (!resampler_)
                resampler_.reset(new Resampler);
            auto input = av_frame_clone(frame);
            auto output = av_frame_alloc();
            // keep same number of bytes per sample
            if (fmt == AV_SAMPLE_FMT_FLT || fmt == AV_SAMPLE_FMT_FLTP)
                output->format = AV_SAMPLE_FMT_S32;
            else
                output->format = AV_SAMPLE_FMT_S64;
            output->sample_rate = input->sample_rate;
            output->channel_layout = input->channel_layout;
            resampler_->resample(input, output);
            av_frame_unref(input);
            av_frame_move_ref(frame, output);
        }
        default:
            return;
        }
    }

    std::ofstream f_;
    size_t dataChunk_;
    size_t length_;
    std::unique_ptr<Resampler> resampler_; // convert from float to integer samples
};

}} // namespace ring::debug
