/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
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
const constexpr unsigned MAX_ACCEL_FAILURES {5};

MediaDemuxer::MediaDemuxer()
    : inputCtx_(avformat_alloc_context())
    , startTime_(AV_NOPTS_VALUE)
{}

MediaDemuxer::~MediaDemuxer()
{
    if (inputCtx_)
        avformat_close_input(&inputCtx_);
    av_dict_free(&options_);
}

const char*
MediaDemuxer::getStatusStr(Status status)
{
    switch (status) {
    case Status::Success:
        return "Success";
    case Status::EndOfFile:
        return "End of file";
    case Status::ReadBufferOverflow:
        return "Read overflow";
    case Status::ReadError:
        return "Read error";
    case Status::FallBack:
        return "Fallback";
    case Status::RestartRequired:
        return "Restart required";
    default:
        return "Undefined";
    }
}

int
MediaDemuxer::openInput(const DeviceParams& params)
{
    inputParams_ = params;
    auto iformat = av_find_input_format(params.format.c_str());

    if (!iformat && !params.format.empty())
        JAMI_WARN("Cannot find format \"%s\"", params.format.c_str());

    if (params.width and params.height) {
        auto sizeStr = fmt::format("{}x{}", params.width, params.height);
        av_dict_set(&options_, "video_size", sizeStr.c_str(), 0);
    }

    if (params.framerate) {
#ifdef _WIN32
        // On windows, framerate settings don't reduce to avrational values
        // that correspond to valid video device formats.
        // e.g. A the rational<double>(10000000, 333333) or 30.000030000
        //      will be reduced by av_reduce to 999991/33333 or 30.00003000003
        //      which cause the device opening routine to fail.
        // So we treat this imprecise reduction and adjust the value,
        // or let dshow choose the framerate, which is, unfortunately,
        // NOT the highest according to our experimentations.
        auto framerate {params.framerate.real()};
        framerate = params.framerate.numerator() / (params.framerate.denominator() + 0.5);
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

    if (!params.pixel_format.empty()) {
        av_dict_set(&options_, "pixel_format", params.pixel_format.c_str(), 0);
    }
    if (!params.window_id.empty()) {
        av_dict_set(&options_, "window_id", params.window_id.c_str(), 0);
    }
    av_dict_set(&options_, "is_area", std::to_string(params.is_area).c_str(), 0);

#if defined(__APPLE__) && TARGET_OS_MAC
    std::string input = params.name;
#else
    std::string input = params.input;
#endif

    JAMI_DBG("Trying to open device %s with format %s, pixel format %s, size %dx%d, rate %lf",
             input.c_str(),
             params.format.c_str(),
             params.pixel_format.c_str(),
             params.width,
             params.height,
             params.framerate.real());

    av_opt_set_int(
        inputCtx_,
        "fpsprobesize",
        1,
        AV_OPT_SEARCH_CHILDREN); // Don't waste time fetching framerate when finding stream info
    int ret = avformat_open_input(&inputCtx_, input.c_str(), iformat, options_ ? &options_ : NULL);

    if (ret) {
        JAMI_ERR("avformat_open_input failed: %s", libav_utils::getError(ret).c_str());
    } else {
        baseWidth_ = inputCtx_->streams[0]->codecpar->width;
        baseHeight_ = inputCtx_->streams[0]->codecpar->height;
        JAMI_DBG("Using format %s and resolution %dx%d",
                 params.format.c_str(),
                 baseWidth_,
                 baseHeight_);
    }

    return ret;
}

int64_t
MediaDemuxer::getDuration() const
{
    return inputCtx_->duration;
}

bool
MediaDemuxer::seekFrame(int, int64_t timestamp)
{
    if (av_seek_frame(inputCtx_, -1, timestamp, AVSEEK_FLAG_BACKWARD) >= 0) {
        clearFrames();
        return true;
    }
    return false;
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

void
MediaDemuxer::setInterruptCallback(int (*cb)(void*), void* opaque)
{
    if (cb) {
        inputCtx_->interrupt_callback.callback = cb;
        inputCtx_->interrupt_callback.opaque = opaque;
    } else {
        inputCtx_->interrupt_callback.callback = 0;
    }
}
void
MediaDemuxer::setNeedFrameCb(std::function<void()> cb)
{
    needFrameCb_ = std::move(cb);
}

void
MediaDemuxer::setFileFinishedCb(std::function<void(bool)> cb)
{
    fileFinishedCb_ = std::move(cb);
}

void
MediaDemuxer::clearFrames()
{
    {
        std::lock_guard<std::mutex> lk {videoBufferMutex_};
        while (!videoBuffer_.empty()) {
            videoBuffer_.pop();
        }
    }
    {
        std::lock_guard<std::mutex> lk {audioBufferMutex_};
        while (!audioBuffer_.empty()) {
            audioBuffer_.pop();
        }
    }
}

void
MediaDemuxer::emitFrame(bool isAudio)
{
    if (isAudio) {
        pushFrameFrom(audioBuffer_, isAudio, audioBufferMutex_);
    } else {
        pushFrameFrom(videoBuffer_, isAudio, videoBufferMutex_);
    }
}

void
MediaDemuxer::pushFrameFrom(
    std::queue<std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>>& buffer,
    bool isAudio,
    std::mutex& mutex)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (buffer.empty()) {
        if (currentState_ == MediaDemuxer::CurrentState::Finished) {
            fileFinishedCb_(isAudio);
        } else {
            needFrameCb_();
        }
        return;
    }
    auto packet = std::move(buffer.front());
    if (!packet) {
        return;
    }
    auto streamIndex = packet->stream_index;
    if (static_cast<unsigned>(streamIndex) >= streams_.size() || streamIndex < 0) {
        return;
    }
    auto& cb = streams_[streamIndex];
    if (!cb) {
        return;
    }
    buffer.pop();
    lock.unlock();
    cb(*packet.get());
}

MediaDemuxer::Status
MediaDemuxer::demuxe()
{
    auto packet = std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>(av_packet_alloc(),
                                                                            [](AVPacket* p) {
                                                                                if (p)
                                                                                    av_packet_free(
                                                                                        &p);
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

    AVStream* stream = inputCtx_->streams[streamIndex];
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        std::lock_guard<std::mutex> lk {videoBufferMutex_};
        videoBuffer_.push(std::move(packet));
        if (videoBuffer_.size() >= 90) {
            return Status::ReadBufferOverflow;
        }
    } else {
        std::lock_guard<std::mutex> lk {audioBufferMutex_};
        audioBuffer_.push(std::move(packet));
        if (audioBuffer_.size() >= 300) {
            return Status::ReadBufferOverflow;
        }
    }
    return Status::Success;
}

void
MediaDemuxer::setIOContext(MediaIOHandle* ioctx)
{
    inputCtx_->pb = ioctx->getContext();
}

MediaDemuxer::Status
MediaDemuxer::decode()
{
    auto packet = std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>(av_packet_alloc(),
                                                                            [](AVPacket* p) {
                                                                                if (p)
                                                                                    av_packet_free(
                                                                                        &p);
                                                                            });
    if (inputParams_.format == "x11grab" || inputParams_.format == "dxgigrab") {
        auto ret = inputCtx_->iformat->read_header(inputCtx_);
        if (ret == AVERROR_EXTERNAL) {
            JAMI_ERR("Couldn't read frame: %s\n", libav_utils::getError(ret).c_str());
            return Status::ReadError;
        }
        auto codecpar = inputCtx_->streams[0]->codecpar;
        if (baseHeight_ != codecpar->height || baseWidth_ != codecpar->width) {
            baseHeight_ = codecpar->height;
            baseWidth_ = codecpar->width;
            inputParams_.height = ((baseHeight_ >> 3) << 3);
            inputParams_.width = ((baseWidth_ >> 3) << 3);
            return Status::RestartRequired;
        }
    }

    int ret = av_read_frame(inputCtx_, packet.get());
    if (ret == AVERROR(EAGAIN)) {
        /*no data available. Calculate time until next frame.
         We do not use the emulated frame mechanism from the decoder because it will affect all
         platforms. With the current implementation, the demuxer will be waiting just in case when
         av_read_frame returns EAGAIN. For some platforms, av_read_frame is blocking and it will
         never happen.
         */
        if (inputParams_.framerate.numerator() == 0)
            return Status::Success;
        rational<double> frameTime = 1e6 / inputParams_.framerate;
        int64_t timeToSleep = lastReadPacketTime_ - av_gettime_relative()
                              + frameTime.real<int64_t>();
        if (timeToSleep <= 0) {
            return Status::Success;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(timeToSleep));
        return Status::Success;
    } else if (ret == AVERROR_EOF) {
        return Status::EndOfFile;
    } else if (ret == AVERROR(EACCES)) {
        return Status::RestartRequired;
    } else if (ret < 0) {
        auto media = inputCtx_->streams[0]->codecpar->codec_type;
        const auto type = media == AVMediaType::AVMEDIA_TYPE_AUDIO
                              ? "AUDIO"
                              : (media == AVMediaType::AVMEDIA_TYPE_VIDEO ? "VIDEO" : "UNSUPPORTED");
        JAMI_ERR("Couldn't read [%s] frame: %s\n", type, libav_utils::getError(ret).c_str());
        return Status::ReadError;
    }

    auto streamIndex = packet->stream_index;
    if (static_cast<unsigned>(streamIndex) >= streams_.size() || streamIndex < 0) {
        return Status::Success;
    }

    lastReadPacketTime_ = av_gettime_relative();

    auto& cb = streams_[streamIndex];
    if (cb) {
        DecodeStatus ret = cb(*packet.get());
        if (ret == DecodeStatus::FallBack)
            return Status::FallBack;
    }
    return Status::Success;
}

MediaDecoder::MediaDecoder(const std::shared_ptr<MediaDemuxer>& demuxer, int index)
    : demuxer_(demuxer)
    , avStream_(demuxer->getStream(index))
{
    demuxer->setStreamCallback(index, [this](AVPacket& packet) { return decode(packet); });
    setupStream();
}

MediaDecoder::MediaDecoder(const std::shared_ptr<MediaDemuxer>& demuxer,
                           int index,
                           MediaObserver observer)
    : demuxer_(demuxer)
    , avStream_(demuxer->getStream(index))
    , callback_(std::move(observer))
{
    demuxer->setStreamCallback(index, [this](AVPacket& packet) { return decode(packet); });
    setupStream();
}

void
MediaDecoder::emitFrame(bool isAudio)
{
    demuxer_->emitFrame(isAudio);
}

MediaDecoder::MediaDecoder()
    : demuxer_(new MediaDemuxer)
{}

MediaDecoder::MediaDecoder(MediaObserver o)
    : demuxer_(new MediaDemuxer)
    , callback_(std::move(o))
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

void
MediaDecoder::flushBuffers()
{
    avcodec_flush_buffers(decoderCtx_);
}

int
MediaDecoder::openInput(const DeviceParams& p)
{
    return demuxer_->openInput(p);
}

void
MediaDecoder::setInterruptCallback(int (*cb)(void*), void* opaque)
{
    demuxer_->setInterruptCallback(cb, opaque);
}

void
MediaDecoder::setIOContext(MediaIOHandle* ioctx)
{
    demuxer_->setIOContext(ioctx);
}

int
MediaDecoder::setup(AVMediaType type)
{
    demuxer_->findStreamInfo();
    auto stream = demuxer_->selectStream(type);
    if (stream < 0) {
        JAMI_ERR("No stream found for type %i", static_cast<int>(type));
        return -1;
    }
    avStream_ = demuxer_->getStream(stream);
    if (avStream_ == nullptr) {
        JAMI_ERR("No stream found at index %i", stream);
        return -1;
    }
    demuxer_->setStreamCallback(stream, [this](AVPacket& packet) { return decode(packet); });
    return setupStream();
}

int
MediaDecoder::setupStream()
{
    int ret = 0;
    avcodec_free_context(&decoderCtx_);

    if (prepareDecoderContext() < 0)
        return -1; // failed

#ifdef RING_ACCEL
    // if there was a fallback to software decoding, do not enable accel
    // it has been disabled already by the video_receive_thread/video_input
    enableAccel_ &= Manager::instance().videoPreferences.getDecodingAccelerated();

    if (enableAccel_ and not fallback_) {
        auto APIs = video::HardwareAccel::getCompatibleAccel(decoderCtx_->codec_id,
                                                             decoderCtx_->width,
                                                             decoderCtx_->height,
                                                             CODEC_DECODER);
        for (const auto& it : APIs) {
            accel_ = std::make_unique<video::HardwareAccel>(it); // save accel
            auto ret = accel_->initAPI(false, nullptr);
            if (ret < 0) {
                accel_.reset();
                continue;
            }
            if (prepareDecoderContext() < 0)
                return -1; // failed
            accel_->setDetails(decoderCtx_);
            decoderCtx_->opaque = accel_.get();
            decoderCtx_->pix_fmt = accel_->getFormat();
            if (avcodec_open2(decoderCtx_, inputDecoder_, &options_) < 0) {
                // Failed to open codec
                JAMI_WARN("Fail to open hardware decoder for %s with %s",
                          avcodec_get_name(decoderCtx_->codec_id),
                          it.getName().c_str());
                avcodec_free_context(&decoderCtx_);
                decoderCtx_ = nullptr;
                accel_.reset();
                continue;
            } else {
                // Succeed to open codec
                JAMI_WARN("Using hardware decoding for %s with %s",
                          avcodec_get_name(decoderCtx_->codec_id),
                          it.getName().c_str());
                break;
            }
        }
    }
#endif

    JAMI_DBG() << "Decoding " << av_get_media_type_string(avStream_->codecpar->codec_type)
               << " using " << inputDecoder_->long_name << " (" << inputDecoder_->name << ")";

    decoderCtx_->thread_count = std::max(1u, std::min(8u, std::thread::hardware_concurrency() / 2));
    if (emulateRate_)
        JAMI_DBG() << "Using framerate emulation";
    startTime_ = av_gettime(); // used to set pts after decoding, and for rate emulation

#ifdef RING_ACCEL
    if (!accel_) {
        JAMI_WARN("Not using hardware decoding for %s", avcodec_get_name(decoderCtx_->codec_id));
        ret = avcodec_open2(decoderCtx_, inputDecoder_, nullptr);
    }
#else
    ret = avcodec_open2(decoderCtx_, inputDecoder_, nullptr);
#endif
    if (ret < 0) {
        JAMI_ERR() << "Could not open codec: " << libav_utils::getError(ret);
        return -1;
    }

    return 0;
}

int
MediaDecoder::prepareDecoderContext()
{
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
    width_ = decoderCtx_->width;
    height_ = decoderCtx_->height;
    decoderCtx_->framerate = avStream_->avg_frame_rate;
    if (avStream_->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (decoderCtx_->framerate.num == 0 || decoderCtx_->framerate.den == 0)
            decoderCtx_->framerate = inputParams_.framerate;
        if (decoderCtx_->framerate.num == 0 || decoderCtx_->framerate.den == 0)
            decoderCtx_->framerate = av_inv_q(decoderCtx_->time_base);
        if (decoderCtx_->framerate.num == 0 || decoderCtx_->framerate.den == 0)
            decoderCtx_->framerate = {30, 1};
    }
    if (avStream_->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (decoderCtx_->codec_id == AV_CODEC_ID_OPUS) {
            av_opt_set_int(decoderCtx_, "decode_fec", fecEnabled_ ? 1 : 0, AV_OPT_SEARCH_CHILDREN);
        }
    }
    return 0;
}

void
MediaDecoder::updateStartTime(int64_t startTime)
{
    startTime_ = startTime;
}

DecodeStatus
MediaDecoder::decode(AVPacket& packet)
{
    int frameFinished = 0;
    auto ret = avcodec_send_packet(decoderCtx_, &packet);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
#ifdef RING_ACCEL
        if (accel_) {
            JAMI_WARN("Decoding error falling back to software");
            fallback_ = true;
            accel_.reset();
            avcodec_flush_buffers(decoderCtx_);
            setupStream();
            return DecodeStatus::FallBack;
        }
#endif
        avcodec_flush_buffers(decoderCtx_);
        setupStream();
        return ret == AVERROR_EOF ? DecodeStatus::Success : DecodeStatus::DecodeError;
    }

#ifdef ENABLE_VIDEO
    auto f = (inputDecoder_->type == AVMEDIA_TYPE_VIDEO)
                 ? std::static_pointer_cast<MediaFrame>(std::make_shared<VideoFrame>())
                 : std::static_pointer_cast<MediaFrame>(std::make_shared<AudioFrame>());
#else
    auto f = std::static_pointer_cast<MediaFrame>(std::make_shared<AudioFrame>());
#endif
    auto frame = f->pointer();
    ret = avcodec_receive_frame(decoderCtx_, frame);
    frame->time_base = decoderCtx_->time_base;
    if (resolutionChangedCallback_) {
        if (decoderCtx_->width != width_ or decoderCtx_->height != height_) {
            JAMI_DBG("Resolution changed from %dx%d to %dx%d",
                     width_,
                     height_,
                     decoderCtx_->width,
                     decoderCtx_->height);
            width_ = decoderCtx_->width;
            height_ = decoderCtx_->height;
            resolutionChangedCallback_(width_, height_);
        }
    }
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        return DecodeStatus::DecodeError;
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
                                      {1, AV_TIME_BASE},
                                      decoderCtx_->time_base,
                                      static_cast<AVRounding>(AV_ROUND_NEAR_INF
                                                              | AV_ROUND_PASS_MINMAX));
        lastTimestamp_ = frame->pts;
        if (emulateRate_ and packetTimestamp != AV_NOPTS_VALUE) {
            auto startTime = avStream_->start_time == AV_NOPTS_VALUE ? 0 : avStream_->start_time;
            rational<double> frame_time = rational<double>(getTimeBase())
                                          * (packetTimestamp - startTime);
            auto target_relative = static_cast<std::int64_t>(frame_time.real() * 1e6);
            auto target_absolute = startTime_ + target_relative;
            if (target_relative < seekTime_) {
                return DecodeStatus::Success;
            }
            // required frame found. Reset seek time
            if (target_relative >= seekTime_) {
                resetSeekTime();
            }
            auto now = av_gettime();
            if (target_absolute > now) {
                std::this_thread::sleep_for(std::chrono::microseconds(target_absolute - now));
            }
        }

        if (callback_)
            callback_(std::move(f));

        if (contextCallback_ && firstDecode_.load()) {
            firstDecode_.exchange(false);
            contextCallback_();
        }
        return DecodeStatus::FrameFinished;
    }
    return DecodeStatus::Success;
}

void
MediaDecoder::setSeekTime(int64_t time)
{
    seekTime_ = time;
}

MediaDemuxer::Status
MediaDecoder::decode()
{
    auto ret = demuxer_->decode();
    if (ret == MediaDemuxer::Status::RestartRequired) {
        avcodec_flush_buffers(decoderCtx_);
        setupStream();
        ret = MediaDemuxer::Status::EndOfFile;
    }
    return ret;
}

#ifdef ENABLE_VIDEO
#ifdef RING_ACCEL
void
MediaDecoder::enableAccel(bool enableAccel)
{
    enableAccel_ = enableAccel;
    emitSignal<libjami::ConfigurationSignal::HardwareDecodingChanged>(enableAccel_);
    if (!enableAccel) {
        accel_.reset();
        if (decoderCtx_)
            decoderCtx_->opaque = nullptr;
    }
}
#endif

DecodeStatus
MediaDecoder::flush()
{
    AVPacket inpacket;
    av_init_packet(&inpacket);

    int frameFinished = 0;
    int ret = 0;
    ret = avcodec_send_packet(decoderCtx_, &inpacket);
    if (ret < 0 && ret != AVERROR(EAGAIN))
        return ret == AVERROR_EOF ? DecodeStatus::Success : DecodeStatus::DecodeError;

    auto result = std::make_shared<MediaFrame>();
    ret = avcodec_receive_frame(decoderCtx_, result->pointer());
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        return DecodeStatus::DecodeError;
    if (ret >= 0)
        frameFinished = 1;

    if (frameFinished) {
        av_packet_unref(&inpacket);
        if (callback_)
            callback_(std::move(result));
        return DecodeStatus::FrameFinished;
    }

    return DecodeStatus::Success;
}
#endif // ENABLE_VIDEO

int
MediaDecoder::getWidth() const
{
    return decoderCtx_->width;
}

int
MediaDecoder::getHeight() const
{
    return decoderCtx_->height;
}

std::string
MediaDecoder::getDecoderName() const
{
    return decoderCtx_->codec->name;
}

rational<double>
MediaDecoder::getFps() const
{
    return {(double) avStream_->avg_frame_rate.num, (double) avStream_->avg_frame_rate.den};
}

rational<unsigned>
MediaDecoder::getTimeBase() const
{
    return {(unsigned) avStream_->time_base.num, (unsigned) avStream_->time_base.den};
}

AVPixelFormat
MediaDecoder::getPixelFormat() const
{
    return decoderCtx_->pix_fmt;
}

int
MediaDecoder::correctPixFmt(int input_pix_fmt)
{
    // https://ffmpeg.org/pipermail/ffmpeg-user/2014-February/020152.html
    int pix_fmt;
    switch (input_pix_fmt) {
    case AV_PIX_FMT_YUVJ420P:
        pix_fmt = AV_PIX_FMT_YUV420P;
        break;
    case AV_PIX_FMT_YUVJ422P:
        pix_fmt = AV_PIX_FMT_YUV422P;
        break;
    case AV_PIX_FMT_YUVJ444P:
        pix_fmt = AV_PIX_FMT_YUV444P;
        break;
    case AV_PIX_FMT_YUVJ440P:
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
