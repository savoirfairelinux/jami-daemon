/*
 *  Copyright (C) 2013-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
#include "client/ring_signal.h"

#include <iostream>
#include <unistd.h>
#include <thread> // hardware_concurrency
#include <chrono>
#include <algorithm>

namespace jami {

// maximum number of packets the jitter buffer can queue
const unsigned jitterBufferMaxSize_ {1500};
// maximum time a packet can be queued
const constexpr auto jitterBufferMaxDelay_ = std::chrono::milliseconds(50);
// maximum number of times accelerated decoding can fail in a row before falling back to software
const constexpr unsigned MAX_ACCEL_FAILURES { 5 };

MediaDemuxer::MediaDemuxer() :
    inputCtx_(avformat_alloc_context()),
    startTime_(AV_NOPTS_VALUE)
{}

MediaDemuxer::~MediaDemuxer()
{
    if (inputCtx_)
        avformat_close_input(&inputCtx_);
    av_dict_free(&options_);
}

int
MediaDemuxer::openInput(const DeviceParams& params)
{
    inputParams_ = params;
    AVInputFormat *iformat = av_find_input_format(params.format.c_str());

    if (!iformat && !params.format.empty())
        JAMI_WARN("Cannot find format \"%s\"", params.format.c_str());

    if (params.width and params.height) {
        std::stringstream ss;
        ss << params.width << "x" << params.height;
        av_dict_set(&options_, "video_size", ss.str().c_str(), 0);
    }

    if (params.framerate) {
#ifdef _WIN32
        // On windows, certain framerate settings don't reduce to avrational values
        // that correspond to valid video device formats.
        // e.g. A the rational<double>(10000000, 333333) or 30.000030000
        //      will be reduced by av_reduce to 999991/33333 or 30.00003000003
        //      which cause the device opening routine to fail.
        // So we treat special cases in which the reduction is imprecise and adjust
        // the value, or let dshow choose the framerate, which is, unfortunately,
        // NOT the highest according to our experimentations.
        auto framerate{ params.framerate.real() };
        if (params.framerate.denominator() == 333333)
            framerate = (int)(params.framerate.real());
        if (params.framerate.denominator() != 4999998)
            av_dict_set(&options_, "framerate", jami::to_string(framerate).c_str(), 0);
#else
        av_dict_set(&options_, "framerate", jami::to_string(params.framerate.real()).c_str(), 0);
#endif
    }

    if (params.offset_x || params.offset_y) {
        av_dict_set(&options_, "offset_x", std::to_string(params.offset_x).c_str(), 0);
        av_dict_set(&options_, "offset_y", std::to_string(params.offset_y).c_str(), 0);
    }
    if (params.channel)
        av_dict_set(&options_, "channel", std::to_string(params.channel).c_str(), 0);
    av_dict_set(&options_, "loop", params.loop.c_str(), 0);
    av_dict_set(&options_, "sdp_flags", params.sdp_flags.c_str(), 0);

    // Set jitter buffer options
    av_dict_set(&options_, "reorder_queue_size", std::to_string(jitterBufferMaxSize_).c_str(), 0);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(jitterBufferMaxDelay_).count();
    av_dict_set(&options_, "max_delay", std::to_string(us).c_str(), 0);

    if(!params.pixel_format.empty()){
        av_dict_set(&options_, "pixel_format", params.pixel_format.c_str(), 0);
    }

#if defined(__APPLE__) && TARGET_OS_MAC
    std::string input = params.name;
#else
    std::string input = params.input;
#endif

    JAMI_DBG("Trying to open device %s with format %s, pixel format %s, size %dx%d, rate %lf", input.c_str(),
                                                        params.format.c_str(), params.pixel_format.c_str(), params.width, params.height, params.framerate.real());

    av_opt_set_int(inputCtx_, "fpsprobesize", 1, AV_OPT_SEARCH_CHILDREN); // Don't waste time fetching framerate when finding stream info
    int ret = avformat_open_input(
        &inputCtx_,
        input.c_str(),
        iformat,
        options_ ? &options_ : NULL);

    if (ret) {
        JAMI_ERR("avformat_open_input failed: %s", libav_utils::getError(ret).c_str());
    } else {
        JAMI_DBG("Using format %s", params.format.c_str());
    }

    return ret;
}

void
MediaDemuxer::findStreamInfo()
{
    if (not streamInfoFound_) {
        inputCtx_->max_analyze_duration = 30 * AV_TIME_BASE;
        int err;
        if ((err = avformat_find_stream_info(inputCtx_, nullptr)) < 0) {
            JAMI_ERR() << "Could not find stream info: " << libav_utils::getError(err);
        }
        streamInfoFound_ = true;
    }
}

int
MediaDemuxer::selectStream(AVMediaType type)
{
    return av_find_best_stream(inputCtx_, type, -1, -1, nullptr, 0);
}

void MediaDemuxer::setInterruptCallback(int (*cb)(void*), void *opaque)
{
    if (cb) {
        inputCtx_->interrupt_callback.callback = cb;
        inputCtx_->interrupt_callback.opaque = opaque;
    } else {
        inputCtx_->interrupt_callback.callback = 0;
    }
}

void MediaDemuxer::setIOContext(MediaIOHandle *ioctx)
{ inputCtx_->pb = ioctx->getContext(); }


MediaDemuxer::Status
MediaDemuxer::decode()
{
    auto packet = std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>(av_packet_alloc(), [](AVPacket* p){
        if (p)
            av_packet_free(&p);
    });

    int ret = av_read_frame(inputCtx_, packet.get());
    if (ret == AVERROR(EAGAIN)) {
        return Status::Success;
    } else if (ret == AVERROR_EOF) {
        return Status::EndOfFile;
    } else if (ret < 0) {
        JAMI_ERR("Couldn't read frame: %s\n", libav_utils::getError(ret).c_str());
        return Status::ReadError;
    }

    auto streamIndex = packet->stream_index;
    if (static_cast<unsigned>(streamIndex) >= streams_.size() || streamIndex < 0) {
        return Status::Success;
    }

    auto& cb = streams_[streamIndex];
    if (cb)
        cb(*packet.get());
    return Status::Success;
}

MediaDecoder::MediaDecoder(const std::shared_ptr<MediaDemuxer>& demuxer, int index)
 : demuxer_(demuxer),
   avStream_(demuxer->getStream(index))
{
    demuxer->setStreamCallback(index, [this](AVPacket& packet) {
        decode(packet);
    });
    setupStream();
}

MediaDecoder::MediaDecoder()
 : demuxer_(new MediaDemuxer)
 {}

MediaDecoder::MediaDecoder(MediaObserver o)
 : demuxer_(new MediaDemuxer), callback_(std::move(o))
 {}

MediaDecoder::~MediaDecoder()
{
#ifdef RING_ACCEL
    if (decoderCtx_ && decoderCtx_->hw_device_ctx)
        av_buffer_unref(&decoderCtx_->hw_device_ctx);
#endif
    if (decoderCtx_)
        avcodec_free_context(&decoderCtx_);
}

int
MediaDecoder::openInput(const DeviceParams& p)
{
    return demuxer_->openInput(p);
}

void
MediaDecoder::setInterruptCallback(int (*cb)(void*), void *opaque)
{
    demuxer_->setInterruptCallback(cb, opaque);
}

void
MediaDecoder::setIOContext(MediaIOHandle *ioctx)
{
    demuxer_->setIOContext(ioctx);
}

int MediaDecoder::setup(AVMediaType type)
{
    demuxer_->findStreamInfo();
    auto stream = demuxer_->selectStream(type);
    if (stream < 0)
        return -1;
    avStream_ = demuxer_->getStream(stream);
    demuxer_->setStreamCallback(stream, [this](AVPacket& packet) {
        decode(packet);
    });
    return setupStream();
}

int
MediaDecoder::setupStream()
{
    int ret = 0;
    avcodec_free_context(&decoderCtx_);

#ifdef RING_ACCEL
    // if there was a fallback to software decoding, do not enable accel
    // it has been disabled already by the video_receive_thread/video_input
    enableAccel_ &= Manager::instance().videoPreferences.getDecodingAccelerated();
#endif

    inputDecoder_ = findDecoder(avStream_->codecpar->codec_id);
    if (!inputDecoder_) {
        JAMI_ERR() << "Unsupported codec";
        return -1;
    }

    decoderCtx_ = avcodec_alloc_context3(inputDecoder_);
    if (!decoderCtx_) {
        JAMI_ERR() << "Failed to create decoder context";
        return -1;
    }
    avcodec_parameters_to_context(decoderCtx_, avStream_->codecpar);
    decoderCtx_->framerate = avStream_->avg_frame_rate;
    if (avStream_->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (decoderCtx_->framerate.num == 0 || decoderCtx_->framerate.den == 0)
            decoderCtx_->framerate = inputParams_.framerate;
        if (decoderCtx_->framerate.num == 0 || decoderCtx_->framerate.den == 0)
            decoderCtx_->framerate = av_inv_q(decoderCtx_->time_base);
        if (decoderCtx_->framerate.num == 0 || decoderCtx_->framerate.den == 0)
            decoderCtx_->framerate = {30, 1};

#ifdef RING_ACCEL
        if (enableAccel_) {
            accel_ = video::HardwareAccel::setupDecoder(decoderCtx_->codec_id,
                decoderCtx_->width, decoderCtx_->height, decoderCtx_->pix_fmt);
            if (accel_) {
                accel_->setDetails(decoderCtx_);
                decoderCtx_->opaque = accel_.get();
            }
        } else if (Manager::instance().videoPreferences.getDecodingAccelerated()) {
            JAMI_WARN() << "Hardware decoding disabled because of previous failure";
        } else {
            JAMI_WARN() << "Hardware decoding disabled by user preference";
        }
#endif
    }

    JAMI_DBG() << "Decoding " << av_get_media_type_string(avStream_->codecpar->codec_type) << " using " << inputDecoder_->long_name << " (" << inputDecoder_->name << ")";

    decoderCtx_->thread_count = std::max(1u, std::min(8u, std::thread::hardware_concurrency()/2));
    if (emulateRate_)
        JAMI_DBG() << "Using framerate emulation";
    startTime_ = av_gettime(); // used to set pts after decoding, and for rate emulation

    ret = avcodec_open2(decoderCtx_, inputDecoder_, nullptr);
    if (ret < 0) {
        JAMI_ERR() << "Could not open codec: " << libav_utils::getError(ret);
        return -1;
    }

    return 0;
}

MediaDecoder::Status
MediaDecoder::decode(AVPacket& packet)
{
    int frameFinished = 0;
    auto ret = avcodec_send_packet(decoderCtx_, &packet);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        return ret == AVERROR_EOF ? Status::Success : Status::DecodeError;
    }

    auto f = (inputDecoder_->type == AVMEDIA_TYPE_VIDEO)
              ? std::static_pointer_cast<MediaFrame>(std::make_shared<VideoFrame>())
              : std::static_pointer_cast<MediaFrame>(std::make_shared<AudioFrame>());
    auto frame = f->pointer();
    ret = avcodec_receive_frame(decoderCtx_, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        return Status::DecodeError;
    }
    if (ret >= 0)
        frameFinished = 1;

    if (frameFinished) {
        // channel layout is needed if frame will be resampled
        if (!frame->channel_layout)
            frame->channel_layout = av_get_default_channel_layout(frame->channels);

        frame->format = (AVPixelFormat) correctPixFmt(frame->format);
        auto packetTimestamp = frame->pts; // in stream time base
        frame->pts = av_rescale_q_rnd(av_gettime() - startTime_,
            {1, AV_TIME_BASE}, decoderCtx_->time_base,
            static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        lastTimestamp_ = frame->pts;

        if (emulateRate_ and packetTimestamp != AV_NOPTS_VALUE) {
            auto frame_time = getTimeBase()*(packetTimestamp - avStream_->start_time);
            auto target = startTime_ + static_cast<std::int64_t>(frame_time.real() * 1e6);
            auto now = av_gettime();
            if (target > now) {
                std::this_thread::sleep_for(std::chrono::microseconds(target - now));
            }
        }
        if (callback_)
            callback_(std::move(f));
        return Status::FrameFinished;
    }

    return Status::Success;
}

MediaDemuxer::Status
MediaDecoder::decode()
{
    return demuxer_->decode();
}

#ifdef ENABLE_VIDEO
#ifdef RING_ACCEL
void
MediaDecoder::enableAccel(bool enableAccel)
{
    enableAccel_ = enableAccel;
    emitSignal<DRing::ConfigurationSignal::HardwareDecodingChanged>(enableAccel_);
    if (!enableAccel) {
        accel_.reset();
        if (decoderCtx_)
            decoderCtx_->opaque = nullptr;
    }
}
#endif

MediaDecoder::Status
MediaDecoder::flush()
{
    AVPacket inpacket;
    av_init_packet(&inpacket);

    int frameFinished = 0;
    int ret = 0;
    ret = avcodec_send_packet(decoderCtx_, &inpacket);
    if (ret < 0 && ret != AVERROR(EAGAIN))
        return ret == AVERROR_EOF ? Status::Success : Status::DecodeError;

    auto result = std::make_shared<MediaFrame>();
    ret = avcodec_receive_frame(decoderCtx_, result->pointer());
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        return Status::DecodeError;
    if (ret >= 0)
        frameFinished = 1;

    if (frameFinished) {
        av_packet_unref(&inpacket);
        if (callback_)
            callback_(std::move(result));
        return Status::FrameFinished;
    }

    return Status::Success;
}
#endif // ENABLE_VIDEO

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

AVPixelFormat MediaDecoder::getPixelFormat() const
{ return decoderCtx_->pix_fmt; }

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

MediaStream
MediaDecoder::getStream(std::string name) const
{
    auto ms = MediaStream(name, decoderCtx_, lastTimestamp_);
#ifdef RING_ACCEL
    // accel_ is null if not using accelerated codecs
    if (accel_)
        ms.format = accel_->getSoftwareFormat();
#endif
    return ms;
}

} // namespace jami
