/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#include "libav_deps.h"
#include "media_io_handle.h"
#include "system_codec_container.h"

#include <opendht/utils.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <ios>
#include <ratio>
#include <string_view>

#include "logger.h"

#warning Debug utilities included in build

using Clock = std::chrono::steady_clock;
using namespace std::literals;

namespace jami {
namespace debug {

/**
 * Ex:
 * Timer t;
 * std::this_thread::sleep_for(std::chrono::milliseconds(10));
 * JAMI_DBG() << "Task took " << t.getDuration<std::chrono::nanoseconds>() << " ns";
 */
class Timer
{
public:
    Timer(std::string_view name) : name_(name), start_(Clock::now()) {}

    ~Timer() {
        print("end"sv);
    }

    template<class Period = std::ratio<1>>
    uint64_t getDuration() const
    {
        auto diff = std::chrono::duration_cast<Period>(Clock::now() - start_);
        return diff.count();
    }

    void print(std::string_view action) const {
        JAMI_DBG() << name_ << ": " << action << " after " << dht::print_duration(Clock::now() - start_);
    }

private:
    std::string_view name_;
    std::chrono::time_point<Clock> start_;
};

/**
 * Audio logger. Writes a wav file from raw PCM or AVFrame. Helps debug what goes wrong with audio.
 */
class WavWriter {
public:
    WavWriter(const char* filename, AVFrame* frame)
    {
        JAMI_WARNING("WavWriter(): {} ({}, {})", filename, av_get_sample_fmt_name((AVSampleFormat)frame->format), frame->sample_rate);
        avformat_alloc_output_context2(&format_ctx_, nullptr, "wav", filename);
        if (!format_ctx_)
            throw std::runtime_error("Failed to allocate output format context");

        AVCodecID codec_id = AV_CODEC_ID_NONE;
        switch (frame->format) {
            case AV_SAMPLE_FMT_U8:
                codec_id = AV_CODEC_ID_PCM_U8;
                break;
            case AV_SAMPLE_FMT_S16:
            case AV_SAMPLE_FMT_S16P:
                codec_id = AV_CODEC_ID_PCM_S16LE;
                break;
            case AV_SAMPLE_FMT_S32:
            case AV_SAMPLE_FMT_S32P:
                codec_id = AV_CODEC_ID_PCM_S32LE;
                break;
            case AV_SAMPLE_FMT_S64:
            case AV_SAMPLE_FMT_S64P:
                codec_id = AV_CODEC_ID_PCM_S64LE;
                break;
            case AV_SAMPLE_FMT_FLT:
            case AV_SAMPLE_FMT_FLTP:
                codec_id = AV_CODEC_ID_PCM_F32LE;
                break;
            case AV_SAMPLE_FMT_DBL:
                codec_id = AV_CODEC_ID_PCM_F64LE;
                break;
            default:
                throw std::runtime_error("Unsupported audio format");
        }

        auto codec = avcodec_find_encoder(codec_id);
        if (!codec)
            throw std::runtime_error("Failed to find audio codec");

        codec_ctx_ = avcodec_alloc_context3(codec);
        if (!codec_ctx_)
            throw std::runtime_error("Failed to allocate audio codec context");

        codec_ctx_->sample_fmt = (AVSampleFormat)frame->format;
        codec_ctx_->ch_layout = frame->ch_layout;
        codec_ctx_->sample_rate = frame->sample_rate;
        if (format_ctx_->oformat->flags & AVFMT_GLOBALHEADER)
            codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(codec_ctx_, codec, nullptr) < 0)
            throw std::runtime_error("Failed to open audio codec");

        stream_ = avformat_new_stream(format_ctx_, codec);
        if (!stream_)
            throw std::runtime_error("Failed to create audio stream");

        if (avcodec_parameters_from_context(stream_->codecpar, codec_ctx_) < 0)
            throw std::runtime_error("Failed to copy codec parameters to stream");

        if (!(format_ctx_->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&format_ctx_->pb, filename, AVIO_FLAG_WRITE) < 0) {
                throw std::runtime_error("Failed to open output file for writing");
            }
        }
        if (avformat_write_header(format_ctx_, nullptr) < 0)
            throw std::runtime_error("Failed to write header to output file");
    }

    void write(AVFrame* frame) {
        int ret = avcodec_send_frame(codec_ctx_, frame);
        if (ret < 0)
            JAMI_ERROR("Error sending a frame to the encoder");
        while (ret >= 0) {
            AVPacket *pkt = av_packet_alloc();
            ret = avcodec_receive_packet(codec_ctx_, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                JAMI_ERROR("Error encoding a frame");
                break;
            }
            pkt->stream_index = stream_->index;
            pkt->pts = lastPts;
            pkt->dts = lastPts;
            lastPts += frame->nb_samples * (int64_t)stream_->time_base.den / (stream_->time_base.num * (int64_t)frame->sample_rate);
            ret = av_write_frame(format_ctx_, pkt);
            if (ret < 0) {
                JAMI_ERROR("Error while writing output packet");
                break;
            }
            av_packet_free(&pkt);
        }
    }

    ~WavWriter() {
        if (codec_ctx_) {
            avcodec_close(codec_ctx_);
            avcodec_free_context(&codec_ctx_);
        }
        if (format_ctx_) {
            av_write_trailer(format_ctx_);
            if (!(format_ctx_->oformat->flags & AVFMT_NOFILE))
                avio_closep(&format_ctx_->pb);
            avformat_free_context(format_ctx_);
        }
    }
private:
    AVFormatContext* format_ctx_ {nullptr};
    AVCodecContext* codec_ctx_ {nullptr};
    AVStream* stream_ {nullptr};
    int64_t lastPts {0};
};

/**
 * Minimally invasive video writer. Writes raw frames. Helps debug what goes wrong with video.
 */
class VideoWriter
{
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
                 av_get_pix_fmt_name(format_),
                 width_,
                 height_,
                 filename_.c_str());
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
        if ((ret = av_image_copy_to_buffer(buffer,
                                           size,
                                           reinterpret_cast<const uint8_t* const*>(f->data),
                                           reinterpret_cast<const int*>(f->linesize),
                                           format_,
                                           width_,
                                           height_,
                                           1))
            < 0) {
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

} // namespace debug
} // namespace jami
