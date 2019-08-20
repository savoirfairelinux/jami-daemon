/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
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
#include "media_io_handle.h"
#include "system_codec_container.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <ios>
#include <ratio>
#include "logger.h"

#warning Debug utilities included in build

using Clock = std::chrono::steady_clock;

namespace jami { namespace debug {

/**
 * Ex:
 * Timer t;
 * std::this_thread::sleep_for(std::chrono::milliseconds(10));
 * JAMI_DBG() << "Task took " << t.getDuration<std::chrono::nanoseconds>() << " ns";
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
    WavWriter(std::string filename, AVFrame* frame)
        : format_(static_cast<AVSampleFormat>(frame->format))
        , channels_(frame->channels)
        , planar_(av_sample_fmt_is_planar(format_))
        , depth_(av_get_bytes_per_sample(format_))
        , stepPerSample_(planar_ ? depth_ : depth_ * channels_)
    {
        std::vector<AVSampleFormat> v{AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_DBL, AV_SAMPLE_FMT_DBLP};
        f_ = std::ofstream(filename, std::ios_base::out | std::ios_base::binary);
        f_.imbue(std::locale::classic());
        f_ << "RIFF----WAVEfmt ";
        if (std::find(v.begin(), v.end(), format_) == v.end()) {
            write(16, 4); // Chunk size
            write(1, 2); // WAVE_FORMAT_PCM
            write(frame->channels, 2);
            write(frame->sample_rate, 4);
            write(frame->sample_rate * depth_ * frame->channels, 4); // Bytes per second
            write(depth_ * frame->channels, 2); // Multi-channel sample size
            write(8 * depth_, 2); // Bits per sample
            f_ << "data";
            dataChunk_ = f_.tellp();
            f_ << "----";
        } else {
            write(18, 4); // Chunk size
            write(3, 2); // Non PCM data
            write(frame->channels, 2);
            write(frame->sample_rate, 4);
            write(frame->sample_rate * depth_ * frame->channels, 4); // Bytes per second
            write(depth_ * frame->channels, 2); // Multi-channel sample size
            write(8 * depth_, 2); // Bits per sample
            write(0, 2); // Extension size
            f_ << "fact";
            write(4, 4); // Chunk size
            factChunk_ = f_.tellp();
            f_ << "----";
            f_ << "data";
            dataChunk_ = f_.tellp();
            f_ << "----";
        }
    }

    ~WavWriter()
    {
        length_ = f_.tellp();
        f_.seekp(dataChunk_);
        write(length_ - dataChunk_ + 4, 4); // bytes_per_sample * channels * nb_samples
        f_.seekp(4);
        write(length_ - 8, 4);
        if (factChunk_) {
            f_.seekp(factChunk_);
            write((length_ - dataChunk_ + 4) / depth_, 4); // channels * nb_samples
        }
        f_.flush();
    }

    template<typename Word>
    void write(Word value, unsigned size = sizeof(Word))
    {
        auto p = reinterpret_cast<unsigned char const *>(&value);
        for (int i = 0; size; --size, ++i)
            f_.put(p[i]);
    }

    void write(AVFrame *frame)
    {
        for (int c = 0; c < frame->channels; ++c) {
            int offset = planar_ ? 0 : depth_ * c;
            for (int i = 0; i < frame->nb_samples; ++i) {
                uint8_t *p = &frame->extended_data[planar_ ? c : 0][i + offset];
                switch (format_) {
                case AV_SAMPLE_FMT_U8:
                case AV_SAMPLE_FMT_U8P:
                    write<uint8_t>(*(uint8_t*)p);
                    break;
                case AV_SAMPLE_FMT_S16:
                case AV_SAMPLE_FMT_S16P:
                    write<int16_t>(*(int16_t*)p);
                    break;
                case AV_SAMPLE_FMT_S32:
                case AV_SAMPLE_FMT_S32P:
                    write<int32_t>(*(int32_t*)p);
                    break;
                case AV_SAMPLE_FMT_S64:
                case AV_SAMPLE_FMT_S64P:
                    write<int64_t>(*(int64_t*)p);
                    break;
                case AV_SAMPLE_FMT_FLT:
                case AV_SAMPLE_FMT_FLTP:
                    write<float>(*(float*)p);
                    break;
                case AV_SAMPLE_FMT_DBL:
                case AV_SAMPLE_FMT_DBLP:
                    write<double>(*(double*)p);
                    break;
                default:
                    break;
                }
            }
        }
        f_.flush();
    }

private:
    std::ofstream f_;
    size_t dataChunk_ {0};
    size_t factChunk_ {0};
    size_t length_ {0};
    AVSampleFormat format_ {AV_SAMPLE_FMT_NONE};
    size_t channels_ {0};
    bool planar_ {false};
    int depth_ {0};
    int stepPerSample_ {0};
};

/**
 * Minimally invasive video writer. Writes raw frames. Helps debug what goes wrong with video.
 */
class VideoWriter {
public:
    VideoWriter(const std::string& filename, AVPixelFormat format, int width, int height)
        : filename_(filename)
        , format_(format)
        , width_(width)
        , height_(height)
    {
        f_ = fopen(filename.c_str(), "wb");
    }

    // so an int (VideoFrame.format()) can be passed without casting
    VideoWriter(const std::string& filename, int format, int width, int height)
        : VideoWriter(filename, static_cast<AVPixelFormat>(format), width, height)
    {}

    ~VideoWriter()
    {
        fclose(f_);
        JAMI_DBG("Play video file with: ffplay -f rawvideo -pixel_format %s -video_size %dx%d %s",
            av_get_pix_fmt_name(format_), width_, height_, filename_.c_str());
    }

    void write(VideoFrame& frame)
    {
        int ret = 0;
        uint8_t* buffer = nullptr;
        auto f = frame.pointer();

        if (format_ != f->format || width_ != f->width || height_ != f->height)
            return;

        int size = av_image_get_buffer_size(format_, width_, height_, 1);
        buffer = reinterpret_cast<uint8_t*>(av_malloc(size));
        if (!buffer) {
            return;
        }
        if ((ret = av_image_copy_to_buffer(buffer, size,
            reinterpret_cast<const uint8_t* const*>(f->data),
            reinterpret_cast<const int*>(f->linesize),
            format_, width_, height_, 1)) < 0) {
            av_freep(&buffer);
            return;
        }

        fwrite(buffer, 1, size, f_);
        av_freep(&buffer);
    }

private:
    FILE* f_;
    std::string filename_;
    AVPixelFormat format_;
    int width_, height_;
};

}} // namespace jami::debug
