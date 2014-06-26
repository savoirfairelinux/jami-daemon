/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

// libav_deps.h must be included first
#include "libav_deps.h"
#include "video_decoder.h"
#include "logger.h"

#include <iostream>
#include <unistd.h>

namespace sfl_video {

using std::string;

VideoDecoder::VideoDecoder() :
    inputCtx_(avformat_alloc_context()),
    startTime_(AV_NOPTS_VALUE),
    lastDts_(AV_NOPTS_VALUE)
{
}

VideoDecoder::~VideoDecoder()
{
    if (decoderCtx_)
        avcodec_close(decoderCtx_);

    if (inputCtx_ and inputCtx_->nb_streams > 0) {
#if LIBAVFORMAT_VERSION_CHECK(53, 17, 0, 25, 0)
        avformat_close_input(&inputCtx_);
#else
        av_close_input_file(inputCtx_);
#endif
    }
}

void
VideoDecoder::extract(const std::map<std::string, std::string>& map, const std::string& key)
{
    auto iter = map.find(key);

    if (iter != map.end())
        av_dict_set(&options_, key.c_str(), iter->second.c_str(), 0);
}

void
VideoDecoder::setOptions(const std::map<std::string, std::string>& options)
{
    extract(options, "framerate");
    extract(options, "video_size");
    extract(options, "channel");
    extract(options, "loop");
#if HAVE_SDP_CUSTOM_IO
    extract(options, "sdp_flags");
#endif
}

int VideoDecoder::openInput(const std::string &source_str,
                            const std::string &format_str)
{
    AVInputFormat *iformat = av_find_input_format(format_str.c_str());

    if (!iformat)
        WARN("Cannot find format \"%s\"", format_str.c_str());

    int ret = avformat_open_input(&inputCtx_, source_str.c_str(), iformat,
                                  options_ ? &options_ : NULL);

    if (ret) {
        char errbuf[64];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("avformat_open_input failed: %s", errbuf);
    } else {
        DEBUG("Using format %s", format_str.c_str());
    }

    return ret;
}

void VideoDecoder::setInterruptCallback(int (*cb)(void*), void *opaque)
{
    if (cb) {
        inputCtx_->interrupt_callback.callback = cb;
        inputCtx_->interrupt_callback.opaque = opaque;
    } else {
        inputCtx_->interrupt_callback.callback = 0;
    }
}

void VideoDecoder::setIOContext(VideoIOHandle *ioctx)
{ inputCtx_->pb = ioctx->getContext(); }

int VideoDecoder::setupFromVideoData()
{
    int ret;

    if (decoderCtx_)
        avcodec_close(decoderCtx_);

    // Increase analyze time to solve synchronization issues between callers.
    static const unsigned MAX_ANALYZE_DURATION = 30; // time in seconds

    inputCtx_->max_analyze_duration = MAX_ANALYZE_DURATION * AV_TIME_BASE;

    DEBUG("Finding stream info");
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
        ERROR("Could not find stream info: %s", errBuf);
        return -1;
    }

    // find the first video stream from the input
    for (size_t i = 0; streamIndex_ == -1 && i < inputCtx_->nb_streams; ++i)
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            streamIndex_ = i;

    if (streamIndex_ == -1) {
        ERROR("Could not find video stream");
        return -1;
    }

    // Get a pointer to the codec context for the video stream
    decoderCtx_ = inputCtx_->streams[streamIndex_]->codec;
    if (decoderCtx_ == 0) {
        ERROR("Decoder context is NULL");
        return -1;
    }

    // find the decoder for the video stream
    inputDecoder_ = avcodec_find_decoder(decoderCtx_->codec_id);
    if (!inputDecoder_) {
        ERROR("Unsupported codec");
        return -1;
    }

    decoderCtx_->thread_count = 1;
    if (emulateRate_) {
        DEBUG("Using framerate emulation");
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
        ERROR("Could not open codec");
        return -1;
    }

    return 0;
}

VideoDecoder::Status
VideoDecoder::decode(VideoFrame& result, VideoPacket& video_packet)
{
    AVPacket *inpacket = video_packet.get();
    int ret = av_read_frame(inputCtx_, inpacket);
    if (ret == AVERROR(EAGAIN)) {
        return Status::Success;
    } else if (ret == AVERROR_EOF) {
        return Status::EOFError;
    } else if (ret < 0) {
        char errbuf[64];
        av_strerror(ret, errbuf, sizeof(errbuf));
        ERROR("Couldn't read frame: %s\n", errbuf);
        return Status::ReadError;
    }

    // is this a packet from the video stream?
    if (inpacket->stream_index != streamIndex_)
        return Status::Success;

    AVFrame *frame = result.get();
    int frameFinished = 0;
    int len = avcodec_decode_video2(decoderCtx_, frame,
                                    &frameFinished, inpacket);
    if (len <= 0)
        return Status::DecodeError;

    if (frameFinished) {
        if (emulateRate_) {
            if (frame->pkt_dts != AV_NOPTS_VALUE) {
                const auto now = std::chrono::system_clock::now();
                const std::chrono::duration<double> seconds = now - lastFrameClock_;
                const double dTB = av_q2d(inputCtx_->streams[streamIndex_]->time_base);
                const double dts_diff = dTB * (frame->pkt_dts - lastDts_);
                const double usDelay = 1e6 * (dts_diff - seconds.count());
                if (usDelay > 0.0) {
#if LIBAVUTIL_VERSION_CHECK(51, 34, 0, 61, 100)
                    av_usleep(usDelay);
#else
                    usleep(usDelay);
#endif
                }
                lastFrameClock_ = now;
                lastDts_ = frame->pkt_dts;
            }
        }
        return Status::FrameFinished;
    }

    return Status::Success;
}

VideoDecoder::Status
VideoDecoder::flush(VideoFrame& result)
{
    AVPacket inpacket;
    memset(&inpacket, 0, sizeof(inpacket));
    av_init_packet(&inpacket);
    inpacket.data = NULL;
    inpacket.size = 0;

    int frameFinished = 0;
    int len = avcodec_decode_video2(decoderCtx_, result.get(),
                                    &frameFinished, &inpacket);
    if (len <= 0)
        return Status::DecodeError;

    if (frameFinished)
        return Status::FrameFinished;

    return Status::Success;
}

int VideoDecoder::getWidth() const
{ return decoderCtx_->width; }

int VideoDecoder::getHeight() const
{ return decoderCtx_->height; }

int VideoDecoder::getPixelFormat() const
{ return libav_utils::sfl_pixel_format(decoderCtx_->pix_fmt); }

}
