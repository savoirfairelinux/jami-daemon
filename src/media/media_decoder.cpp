/*
 *  Copyright (C) 2013-2018 Savoir-faire Linux Inc.
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
#include "decoder_finder.h"
#include "manager.h"

#ifdef RING_ACCEL
#include "video/accel.h"
#endif

#include "string_utils.h"
#include "logger.h"

#include <iostream>
#include <unistd.h>
#include <thread> // hardware_concurrency
#include <chrono>
#include <algorithm>

namespace ring {

// maximum number of packets the jitter buffer can queue
const unsigned jitterBufferMaxSize_ {1500};
// maximum time a packet can be queued
const constexpr auto jitterBufferMaxDelay_ = std::chrono::milliseconds(50);
// maximum number of times accelerated decoding can fail in a row before falling back to software
const constexpr unsigned MAX_ACCEL_FAILURES { 5 };

MediaDecoder::MediaDecoder() :
    inputCtx_(avformat_alloc_context()),
    startTime_(AV_NOPTS_VALUE)
{
}

MediaDecoder::~MediaDecoder()
{
#ifdef RING_ACCEL
    if (decoderCtx_ && decoderCtx_->hw_device_ctx)
        av_buffer_unref(&decoderCtx_->hw_device_ctx);
#endif
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
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(jitterBufferMaxDelay_).count();
    av_dict_set(&options_, "max_delay",ring::to_string(us).c_str(), 0);

    if(!params.pixel_format.empty()){
        av_dict_set(&options_, "pixel_format", params.pixel_format.c_str(), 0);
    }
    RING_DBG("Trying to open device %s with format %s, pixel format %s, size %dx%d, rate %lf", params.input.c_str(),
                                                        params.format.c_str(), params.pixel_format.c_str(), params.width, params.height, params.framerate.real());

#ifdef RING_ACCEL
    // if there was a fallback to software decoding, do not enable accel
    // it has been disabled already by the video_receive_thread/video_input
    enableAccel_ &= Manager::instance().getDecodingAccelerated();
#endif

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
    // Use AVDictionary to send extra arguments to setupStream, since video setup doesn't need them
    av_dict_set_int(&options_, "nb_channels", format.nb_channels, 0);
    av_dict_set_int(&options_, "sample_rate", format.sample_rate, 0);
    return setupStream(AVMEDIA_TYPE_AUDIO);
}

int
MediaDecoder::setupStream(AVMediaType mediaType)
{
    int ret = 0;
    std::string streamType = av_get_media_type_string(mediaType);

    avcodec_free_context(&decoderCtx_);

    // Increase analyze time to solve synchronization issues between callers.
    static const unsigned MAX_ANALYZE_DURATION = 30;
    inputCtx_->max_analyze_duration = MAX_ANALYZE_DURATION * AV_TIME_BASE;

    // If fallback from accel, don't check for stream info, it's already done
    if (!fallback_) {
        RING_DBG() << "Finding " << streamType << " stream info";
        if ((ret = avformat_find_stream_info(inputCtx_, nullptr)) < 0) {
            char errBuf[64];
            av_strerror(ret, errBuf, sizeof(errBuf));

            // Always fail here
            RING_ERR() << "Could not find " << streamType << " stream info: " << errBuf;
            return -1;
        }
    }

    for (size_t i = 0; streamIndex_ == -1 && i < inputCtx_->nb_streams; ++i) {
        if (inputCtx_->streams[i]->codecpar->codec_type == mediaType) {
            streamIndex_ = i;
        }
    }

    if (streamIndex_ < 0) {
        RING_ERR() << "No " << streamType << " stream found";
        return -1;
    }

    avStream_ = inputCtx_->streams[streamIndex_];

    inputDecoder_ = findDecoder(avStream_->codecpar->codec_id);
    if (!inputDecoder_) {
        RING_ERR() << "Unsupported codec";
        return -1;
    }

    decoderCtx_ = avcodec_alloc_context3(inputDecoder_);
    if (!decoderCtx_) {
        RING_ERR() << "Failed to create decoder context";
        return -1;
    }
    avcodec_parameters_to_context(decoderCtx_, avStream_->codecpar);

    decoderCtx_->thread_count = std::max(1u, std::min(8u, std::thread::hardware_concurrency()/2));
    if (mediaType == AVMEDIA_TYPE_AUDIO) {
        decoderCtx_->channels = std::stoi(av_dict_get(options_, "nb_channels", nullptr, 0)->value);
        decoderCtx_->sample_rate = std::stoi(av_dict_get(options_, "sample_rate", nullptr, 0)->value);
    }

    if (emulateRate_) {
        RING_DBG() << "Using framerate emulation";
        startTime_ = av_gettime();
    }

#ifdef RING_ACCEL
    if (enableAccel_) {
        accel_ = video::setupHardwareDecoding(decoderCtx_);
        decoderCtx_->opaque = &accel_;
    } else if (Manager::instance().getDecodingAccelerated()) {
        RING_WARN() << "Hardware accelerated decoding disabled because of previous failure";
    } else {
        RING_WARN() << "Hardware accelerated decoding disabled by user preference";
    }
#endif

    RING_DBG() << "Decoding " << streamType << " using " << inputDecoder_->long_name << " (" << inputDecoder_->name << ")";

    ret = avcodec_open2(decoderCtx_, inputDecoder_, nullptr);
    if (ret < 0) {
        char errBuf[64];
        av_strerror(ret, errBuf, sizeof(errBuf));
        RING_ERR() << "Could not open codec: " << errBuf;
        return -1;
    }

    return 0;
}

#ifdef RING_VIDEO
int MediaDecoder::setupFromVideoData()
{
    return setupStream(AVMEDIA_TYPE_VIDEO);
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
    ret = avcodec_send_packet(decoderCtx_, &inpacket);
    if (ret < 0) {
        return ret == AVERROR_EOF ? Status::Success : Status::DecodeError;
    }
    ret = avcodec_receive_frame(decoderCtx_, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        return Status::DecodeError;
    }
    if (ret >= 0)
        frameFinished = 1;

    av_packet_unref(&inpacket);

    if (frameFinished) {
        frame->format = (AVPixelFormat) correctPixFmt(frame->format);
#ifdef RING_ACCEL
        if (!accel_.name.empty()) {
            ret = video::transferFrameData(accel_, decoderCtx_, result);
            if (ret < 0) {
                ++accelFailures_;
                if (accelFailures_ >= MAX_ACCEL_FAILURES) {
                    RING_ERR("Hardware decoding failure");
                    accelFailures_ = 0; // reset error count for next time
                    fallback_ = true;
                    return Status::RestartRequired;
                }
            }
        }
#endif
        if (emulateRate_ and frame->pts != AV_NOPTS_VALUE) {
            auto frame_time = getTimeBase()*(frame->pts - avStream_->start_time);
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
        ret = avcodec_send_packet(decoderCtx_, &inpacket);
        if (ret < 0)
            return ret == AVERROR_EOF ? Status::Success : Status::DecodeError;

    ret = avcodec_receive_frame(decoderCtx_, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        return Status::DecodeError;
    if (ret >= 0)
        frameFinished = 1;

    if (frameFinished) {
        av_packet_unref(&inpacket);
        if (emulateRate_ and frame->pts != AV_NOPTS_VALUE) {
            auto frame_time = getTimeBase()*(frame->pts - avStream_->start_time);
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
#ifdef RING_ACCEL
void
MediaDecoder::enableAccel(bool enableAccel)
{
    enableAccel_ = enableAccel;
    if (!enableAccel) {
        accel_ = {};
        if (decoderCtx_->hw_device_ctx)
            av_buffer_unref(&decoderCtx_->hw_device_ctx);
        if (decoderCtx_)
            decoderCtx_->opaque = nullptr;
    }
}
#endif

MediaDecoder::Status
MediaDecoder::flush(VideoFrame& result)
{
    AVPacket inpacket;
    av_init_packet(&inpacket);

    int frameFinished = 0;
    int ret = 0;
    ret = avcodec_send_packet(decoderCtx_, &inpacket);
    if (ret < 0)
        return ret == AVERROR_EOF ? Status::Success : Status::DecodeError;

    ret = avcodec_receive_frame(decoderCtx_, result.pointer());
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        return Status::DecodeError;
    if (ret >= 0)
        frameFinished = 1;

    if (frameFinished) {
        av_packet_unref(&inpacket);
#ifdef RING_ACCEL
        // flush is called when closing the stream
        // so don't restart the media decoder
        if (!accel_.name.empty() && accelFailures_ < MAX_ACCEL_FAILURES)
            video::transferFrameData(accel_, decoderCtx_, result);
#endif
        return Status::FrameFinished;
    }

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
