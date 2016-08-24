/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "media_decoder.h"
#include "media_device.h"
#include "media_buffer.h"
#include "media_io_handle.h"
#include "audio/audiobuffer.h"
#include "audio/ringbuffer.h"
#include "audio/resampler.h"

#include "string_utils.h"
#include "logger.h"

#include <iostream>
#include <unistd.h>
#include <thread> // hardware_concurrency

namespace ring {

using std::string;

MediaDecoder::MediaDecoder() :
    inputCtx_(avformat_alloc_context()),
    startTime_(AV_NOPTS_VALUE)
{
}

MediaDecoder::~MediaDecoder()
{
    if (decoderCtx_)
        avcodec_close(decoderCtx_);
    if (inputCtx_)
        avformat_close_input(&inputCtx_);

    av_dict_free(&options_);
}

int MediaDecoder::openInput(const DeviceParams& params)
{
    AVInputFormat *iformat = av_find_input_format(params.format.c_str());

    if (!iformat)
        RING_WARN("Cannot find format \"%s\"", params.format.c_str());

    if (params.width and params.height) {
        std::stringstream ss;
        ss << params.width << "x" << params.height;
        av_dict_set(&options_, "video_size", ss.str().c_str(), 0);
    }
#ifndef _WIN32
    // on windows, framerate setting can lead to a failure while opening device
    // despite investigations, we didn't found a proper solution
    // we let dshow choose the framerate, which is the highest according to our experimentations
    if (params.framerate)
        av_dict_set(&options_, "framerate", ring::to_string(params.framerate.real()).c_str(), 0);
#endif
    if (params.offset_x || params.offset_y) {
        av_dict_set(&options_, "offset_x", ring::to_string(params.offset_x).c_str(), 0);
        av_dict_set(&options_, "offset_y", ring::to_string(params.offset_y).c_str(), 0);
    }
    if (params.channel)
        av_dict_set(&options_, "channel", ring::to_string(params.channel).c_str(), 0);
    av_dict_set(&options_, "loop", params.loop.c_str(), 0);
    av_dict_set(&options_, "sdp_flags", params.sdp_flags.c_str(), 0);

    // Set jitter buffer options
    av_dict_set(&options_, "reorder_queue_size",ring::to_string(jitterBufferMaxSize_).c_str(), 0);
    av_dict_set(&options_, "max_delay",ring::to_string(jitterBufferMaxDelay_).c_str(), 0);

    if(!params.pixel_format.empty()){
        av_dict_set(&options_, "pixel_format", params.pixel_format.c_str(), 0);
    }
    RING_DBG("Trying to open device %s with format %s, pixel format %s, size %dx%d, rate %lf", params.input.c_str(),
                                                        params.format.c_str(), params.pixel_format.c_str(), params.width, params.height, params.framerate.real());
    int ret = avformat_open_input(
        &inputCtx_,
        params.input.c_str(),
        iformat,
        options_ ? &options_ : NULL);

    if (ret) {
        char errbuf[64];
        av_strerror(ret, errbuf, sizeof(errbuf));
        RING_ERR("avformat_open_input failed: %s", errbuf);
    } else {
        RING_DBG("Using format %s", params.format.c_str());
    }

    return ret;
}

void MediaDecoder::setInterruptCallback(int (*cb)(void*), void *opaque)
{
    if (cb) {
        inputCtx_->interrupt_callback.callback = cb;
        inputCtx_->interrupt_callback.opaque = opaque;
    } else {
        inputCtx_->interrupt_callback.callback = 0;
    }
}

void MediaDecoder::setIOContext(MediaIOHandle *ioctx)
{ inputCtx_->pb = ioctx->getContext(); }

int MediaDecoder::setupFromAudioData(const AudioFormat format)
{
    int ret;

    if (decoderCtx_)
        avcodec_close(decoderCtx_);

    // Increase analyze time to solve synchronization issues between callers.
    static const unsigned MAX_ANALYZE_DURATION = 30; // time in seconds

    inputCtx_->max_analyze_duration = MAX_ANALYZE_DURATION * AV_TIME_BASE;

    RING_DBG("Finding stream info");
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    ret = av_find_stream_info(inputCtx_);
#else
    ret = avformat_find_stream_info(inputCtx_, NULL);
#endif
    RING_DBG("Finding stream info DONE");

    if (ret < 0) {
        // workaround for this bug:
        // http://patches.libav.org/patch/22541/
        if (ret == -1)
            ret = AVERROR_INVALIDDATA;
        char errBuf[64] = {0};
        // print nothing for unknown errors
        if (av_strerror(ret, errBuf, sizeof errBuf) < 0)
            errBuf[0] = '\0';

        // always fail here
        RING_ERR("Could not find stream info: %s", errBuf);
        return -1;
    }

    // find the first audio stream from the input
    for (size_t i = 0; streamIndex_ == -1 && i < inputCtx_->nb_streams; ++i)
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
            streamIndex_ = i;

    if (streamIndex_ == -1) {
        RING_ERR("Could not find audio stream");
        return -1;
    }

    // Get a pointer to the codec context for the video stream
    avStream_ = inputCtx_->streams[streamIndex_];
    decoderCtx_ = avStream_->codec;
    if (decoderCtx_ == 0) {
        RING_ERR("Decoder context is NULL");
        return -1;
    }

    // find the decoder for the audio stream
    inputDecoder_ = avcodec_find_decoder(decoderCtx_->codec_id);
    if (!inputDecoder_) {
        RING_ERR("Unsupported codec");
        return -1;
    }

    decoderCtx_->thread_count = std::thread::hardware_concurrency();
    decoderCtx_->channels = format.nb_channels;
    decoderCtx_->sample_rate = format.sample_rate;

    RING_DBG("Audio decoding using %s with %s",
             inputDecoder_->name, format.toString().c_str());

    if (emulateRate_) {
        RING_DBG("Using framerate emulation");
        startTime_ = av_gettime();
    }

#if LIBAVCODEC_VERSION_MAJOR >= 55
    decoderCtx_->refcounted_frames = 1;
#endif
    ret = avcodec_open2(decoderCtx_, inputDecoder_, NULL);
    if (ret) {
        RING_ERR("Could not open codec");
        return -1;
    }

    return 0;
}

#ifdef RING_VIDEO
int MediaDecoder::setupFromVideoData()
{
    int ret;

    if (decoderCtx_)
        avcodec_close(decoderCtx_);

    // Increase analyze time to solve synchronization issues between callers.
    static const unsigned MAX_ANALYZE_DURATION = 30; // time in seconds

    inputCtx_->max_analyze_duration = MAX_ANALYZE_DURATION * AV_TIME_BASE;

    RING_DBG("Finding stream info");
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    ret = av_find_stream_info(inputCtx_);
#else
    ret = avformat_find_stream_info(inputCtx_, NULL);
#endif
    if (ret < 0) {
        // workaround for this bug:
        // http://patches.libav.org/patch/22541/
        if (ret == -1)
            ret = AVERROR_INVALIDDATA;
        char errBuf[64] = {0};
        // print nothing for unknown errors
        if (av_strerror(ret, errBuf, sizeof errBuf) < 0)
            errBuf[0] = '\0';

        // always fail here
        RING_ERR("Could not find stream info: %s", errBuf);
        return -1;
    }

    // find the first video stream from the input
    for (size_t i = 0; streamIndex_ == -1 && i < inputCtx_->nb_streams; ++i)
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            streamIndex_ = i;

    if (streamIndex_ == -1) {
        RING_ERR("Could not find video stream");
        return -1;
    }

    // Get a pointer to the codec context for the video stream
    avStream_ = inputCtx_->streams[streamIndex_];
    decoderCtx_ = avStream_->codec;
    if (decoderCtx_ == 0) {
        RING_ERR("Decoder context is NULL");
        return -1;
    }

    decoderCtx_->thread_count = std::thread::hardware_concurrency();

    // find the decoder for the video stream
    inputDecoder_ = avcodec_find_decoder(decoderCtx_->codec_id);
    if (!inputDecoder_) {
        RING_ERR("Unsupported codec");
        return -1;
    }

    if (emulateRate_) {
        RING_DBG("Using framerate emulation");
        startTime_ = av_gettime();
    }

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    ret = avcodec_open(decoderCtx_, inputDecoder_);
#else
#   if LIBAVCODEC_VERSION_MAJOR >= 55
    decoderCtx_->refcounted_frames = 1;
#   endif
    ret = avcodec_open2(decoderCtx_, inputDecoder_, NULL);
#endif
    if (ret) {
        RING_ERR("Could not open codec");
        return -1;
    }

    return 0;
}

MediaDecoder::Status
MediaDecoder::decode(VideoFrame& result)
{
    AVPacket inpacket;
    av_init_packet(&inpacket);
    int ret = av_read_frame(inputCtx_, &inpacket);
    if (ret == AVERROR(EAGAIN)) {
        return Status::Success;
    } else if (ret == AVERROR_EOF) {
        return Status::EOFError;
    } else if (ret < 0) {
        char errbuf[64];
        av_strerror(ret, errbuf, sizeof(errbuf));
        RING_ERR("Couldn't read frame: %s\n", errbuf);
        return Status::ReadError;
    }

    // is this a packet from the video stream?
    if (inpacket.stream_index != streamIndex_) {
        av_packet_unref(&inpacket);
        return Status::Success;
    }

    auto frame = result.pointer();
    int frameFinished = 0;
    int len = avcodec_decode_video2(decoderCtx_, frame,
                                    &frameFinished, &inpacket);

    av_packet_unref(&inpacket);

    if (len <= 0)
        return Status::DecodeError;

    if (frameFinished) {
        frame->format = (AVPixelFormat) correctPixFmt(frame->format);
        if (emulateRate_ and frame->pkt_pts != AV_NOPTS_VALUE) {
            auto frame_time = getTimeBase()*(frame->pkt_pts - avStream_->start_time);
            auto target = startTime_ + static_cast<std::int64_t>(frame_time.real() * 1e6);
            auto now = av_gettime();
            if (target > now) {
                std::this_thread::sleep_for(std::chrono::microseconds(target - now));
            }
        }
        return Status::FrameFinished;
    }

    return Status::Success;
}
#endif // RING_VIDEO

MediaDecoder::Status
MediaDecoder::decode(const AudioFrame& decodedFrame)
{
    const auto frame = decodedFrame.pointer();

    AVPacket inpacket;
    av_init_packet(&inpacket);

   int ret = av_read_frame(inputCtx_, &inpacket);
    if (ret == AVERROR(EAGAIN)) {
        return Status::Success;
    } else if (ret == AVERROR_EOF) {
        return Status::EOFError;
    } else if (ret < 0) {
        char errbuf[64];
        av_strerror(ret, errbuf, sizeof(errbuf));
        RING_ERR("Couldn't read frame: %s\n", errbuf);
        return Status::ReadError;
    }

    // is this a packet from the audio stream?
    if (inpacket.stream_index != streamIndex_) {
        av_packet_unref(&inpacket);
        return Status::Success;
    }

    int frameFinished = 0;
    int len = avcodec_decode_audio4(decoderCtx_, frame,
                                    &frameFinished, &inpacket);
    av_packet_unref(&inpacket);

    if (len <= 0) {
        return Status::DecodeError;
    }

    if (frameFinished) {
        if (emulateRate_ and frame->pkt_pts != AV_NOPTS_VALUE) {
            auto frame_time = getTimeBase()*(frame->pkt_pts - avStream_->start_time);
            auto target = startTime_ + static_cast<std::int64_t>(frame_time.real() * 1e6);
            auto now = av_gettime();
            if (target > now) {
                std::this_thread::sleep_for(std::chrono::microseconds(target - now));
            }
        }
        return Status::FrameFinished;
    }

    return Status::Success;
}

#ifdef RING_VIDEO
MediaDecoder::Status
MediaDecoder::flush(VideoFrame& result)
{
    AVPacket inpacket;
    av_init_packet(&inpacket);

    int frameFinished = 0;
    auto len = avcodec_decode_video2(decoderCtx_, result.pointer(),
                                    &frameFinished, &inpacket);

    av_packet_unref(&inpacket);

    if (len <= 0)
        return Status::DecodeError;

    if (frameFinished)
        return Status::FrameFinished;

    return Status::Success;
}
#endif // RING_VIDEO

int MediaDecoder::getWidth() const
{ return decoderCtx_->width; }

int MediaDecoder::getHeight() const
{ return decoderCtx_->height; }

std::string MediaDecoder::getDecoderName() const
{ return decoderCtx_->codec->name; }

rational<double>
MediaDecoder::getFps() const
{
    return {(double)avStream_->avg_frame_rate.num,
            (double)avStream_->avg_frame_rate.den};
}

rational<unsigned>
MediaDecoder::getTimeBase() const
{
    return {(unsigned)avStream_->time_base.num,
            (unsigned)avStream_->time_base.den};
}

int MediaDecoder::getPixelFormat() const
{ return libav_utils::ring_pixel_format(decoderCtx_->pix_fmt); }

void
MediaDecoder::writeToRingBuffer(const AudioFrame& decodedFrame,
                                RingBuffer& rb, const AudioFormat outFormat)
{
    const auto libav_frame = decodedFrame.pointer();
    decBuff_.setFormat(AudioFormat{
        (unsigned) libav_frame->sample_rate,
        (unsigned) decoderCtx_->channels
    });
    decBuff_.resize(libav_frame->nb_samples);

    if ( decoderCtx_->sample_fmt == AV_SAMPLE_FMT_FLTP ) {
        decBuff_.convertFloatPlanarToSigned16(libav_frame->extended_data,
                                         libav_frame->nb_samples,
                                         decoderCtx_->channels);
    } else if ( decoderCtx_->sample_fmt == AV_SAMPLE_FMT_S16 ) {
        decBuff_.deinterleave(reinterpret_cast<const AudioSample*>(libav_frame->data[0]),
                         libav_frame->nb_samples, decoderCtx_->channels);
    }
    if ((unsigned)libav_frame->sample_rate != outFormat.sample_rate) {
        if (!resampler_) {
            RING_DBG("Creating audio resampler");
            resampler_.reset(new Resampler(outFormat));
        }
        resamplingBuff_.setFormat({(unsigned) outFormat.sample_rate, (unsigned) decoderCtx_->channels});
        resamplingBuff_.resize(libav_frame->nb_samples);
        resampler_->resample(decBuff_, resamplingBuff_);
        rb.put(resamplingBuff_);
    } else {
        rb.put(decBuff_);
    }
}

int
MediaDecoder::correctPixFmt(int input_pix_fmt) {

    //https://ffmpeg.org/pipermail/ffmpeg-user/2014-February/020152.html
    int pix_fmt;
    switch (input_pix_fmt) {
    case AV_PIX_FMT_YUVJ420P :
        pix_fmt = AV_PIX_FMT_YUV420P;
        break;
    case AV_PIX_FMT_YUVJ422P  :
        pix_fmt = AV_PIX_FMT_YUV422P;
        break;
    case AV_PIX_FMT_YUVJ444P   :
        pix_fmt = AV_PIX_FMT_YUV444P;
        break;
    case AV_PIX_FMT_YUVJ440P :
        pix_fmt = AV_PIX_FMT_YUV440P;
        break;
    default:
        pix_fmt = input_pix_fmt;
        break;
    }
    return pix_fmt;
}

} // namespace ring
