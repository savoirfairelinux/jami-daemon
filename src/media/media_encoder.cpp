/*
 *  Copyright (C) 2013-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Eloi Bail <Eloi.Bail@savoirfairelinux.com>
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "media_codec.h"
#include "media_encoder.h"
#include "media_buffer.h"

#include "client/ring_signal.h"
#include "fileutils.h"
#include "logger.h"
#include "manager.h"
#include "string_utils.h"
#include "system_codec_container.h"

#ifdef RING_ACCEL
#include "video/accel.h"
#endif

extern "C" {
#include <libavutil/parseutils.h>
}

#include <algorithm>
#include <fstream>
#include <iostream>
#include <json/json.h>
#include <sstream>
#include <thread> // hardware_concurrency

// Define following line if you need to debug libav SDP
//#define DEBUG_SDP 1

namespace jami {

constexpr double LOGREG_PARAM_A {114.40432};
constexpr double LOGREG_PARAM_B {-6.049181};

MediaEncoder::MediaEncoder()
    : outputCtx_(avformat_alloc_context())
{}

MediaEncoder::~MediaEncoder()
{
    if (outputCtx_) {
        if (outputCtx_->priv_data)
            av_write_trailer(outputCtx_);
        for (auto encoderCtx : encoders_) {
            if (encoderCtx) {
#ifndef _MSC_VER
                avcodec_free_context(&encoderCtx);
#else
                avcodec_close(encoderCtx);
#endif
            }
        }
        avformat_free_context(outputCtx_);
    }
    av_dict_free(&options_);
}

void
MediaEncoder::setOptions(const MediaStream& opts)
{
    if (!opts.isValid()) {
        JAMI_ERR() << "Invalid options";
        return;
    }

    if (opts.isVideo) {
        videoOpts_ = opts;
        // Make sure width and height are even (required by x264)
        // This is especially for image/gif streaming, as video files and cameras usually have even resolutions
        videoOpts_.width -= videoOpts_.width % 2;
        videoOpts_.height -= videoOpts_.height % 2;
        if (not videoOpts_.frameRate)
            videoOpts_.frameRate = 30;
    } else {
        audioOpts_ = opts;
    }
}

void
MediaEncoder::setOptions(const MediaDescription& args)
{
    libav_utils::setDictValue(&options_, "payload_type", std::to_string(args.payload_type));
    libav_utils::setDictValue(&options_, "max_rate", std::to_string(args.codec->bitrate));
    libav_utils::setDictValue(&options_, "crf", std::to_string(args.codec->quality));

    if (not args.parameters.empty())
        libav_utils::setDictValue(&options_, "parameters", args.parameters);
}

void
MediaEncoder::setMetadata(const std::string& title, const std::string& description)
{
    if (not title.empty())
        libav_utils::setDictValue(&outputCtx_->metadata, "title", title);
    if (not description.empty())
        libav_utils::setDictValue(&outputCtx_->metadata, "description", description);
}

void
MediaEncoder::setInitSeqVal(uint16_t seqVal)
{
    //only set not default value (!=0)
    if (seqVal != 0)
        av_opt_set_int(outputCtx_, "seq", seqVal, AV_OPT_SEARCH_CHILDREN);
}

uint16_t
MediaEncoder::getLastSeqValue()
{
    int64_t retVal;
    if (av_opt_get_int(outputCtx_, "seq", AV_OPT_SEARCH_CHILDREN, &retVal) >= 0)
        return (uint16_t)retVal;
    else
        return 0;
}

void
MediaEncoder::openOutput(const std::string& filename, const std::string& format)
{
    avformat_free_context(outputCtx_);
    if (format.empty())
        avformat_alloc_output_context2(&outputCtx_, nullptr, nullptr, filename.c_str());
    else
        avformat_alloc_output_context2(&outputCtx_, nullptr, format.c_str(), filename.c_str());

#ifdef RING_ACCEL
    enableAccel_ = Manager::instance().videoPreferences.getEncodingAccelerated();
#endif
}

int
MediaEncoder::addStream(const SystemCodecInfo& systemCodecInfo)
{
    if (systemCodecInfo.mediaType == MEDIA_AUDIO) {
        audioCodec_ = systemCodecInfo.name;
        return initStream(systemCodecInfo, nullptr);
    } else {
        videoCodec_ = systemCodecInfo.name;
        // TODO only support 1 audio stream and 1 video stream per encoder
        if (audioOpts_.isValid())
            return 1; // stream will be added to AVFormatContext after audio stream
        else
            return 0; // only a video stream
    }
}

int
MediaEncoder::initStream(const std::string& codecName, AVBufferRef* framesCtx)
{
    const auto codecInfo = getSystemCodecContainer()->searchCodecByName(codecName, MEDIA_ALL);
    if (codecInfo)
        return initStream(*codecInfo, framesCtx);
    else
        return -1;
}

int
MediaEncoder::initStream(const SystemCodecInfo& systemCodecInfo, AVBufferRef* framesCtx)
{
    AVCodec* outputCodec = nullptr;
    AVCodecContext* encoderCtx = nullptr;
#ifdef RING_ACCEL
    if (systemCodecInfo.mediaType == MEDIA_VIDEO) {
        if (enableAccel_) {
            if (accel_ = video::HardwareAccel::setupEncoder(
                static_cast<AVCodecID>(systemCodecInfo.avcodecId),
                videoOpts_.width, videoOpts_.height, framesCtx)) {
                outputCodec = avcodec_find_encoder_by_name(accel_->getCodecName().c_str());
            }
        } else {
            JAMI_WARN() << "Hardware encoding disabled";
        }
    }
#endif

    if (!outputCodec) {
        /* find the video encoder */
        if (systemCodecInfo.avcodecId == AV_CODEC_ID_H263)
            // For H263 encoding, we force the use of AV_CODEC_ID_H263P (H263-1998)
            // H263-1998 can manage all frame sizes while H263 don't
            // AV_CODEC_ID_H263 decoder will be used for decoding
            outputCodec = avcodec_find_encoder(AV_CODEC_ID_H263P);
        else
            outputCodec = avcodec_find_encoder(static_cast<AVCodecID>(systemCodecInfo.avcodecId));
        if (!outputCodec) {
            JAMI_ERR("Encoder \"%s\" not found!", systemCodecInfo.name.c_str());
            throw MediaEncoderException("No output encoder");
        }
    }

    encoderCtx = prepareEncoderContext(outputCodec, systemCodecInfo.mediaType == MEDIA_VIDEO);
    encoders_.push_back(encoderCtx);

#ifdef RING_ACCEL
    if (accel_) {
        accel_->setDetails(encoderCtx);
        encoderCtx->opaque = accel_.get();
    }
#endif

    uint64_t maxBitrate = 1000 * std::atoi(libav_utils::getDictValue(options_, "max_rate"));
    uint8_t crf = (uint8_t) std::round(LOGREG_PARAM_A + log(pow(maxBitrate, LOGREG_PARAM_B)));     // CRF = A + B*ln(maxBitrate)
    uint64_t bufSize = 2 * maxBitrate;

    /* let x264 preset override our encoder settings */
    if (systemCodecInfo.avcodecId == AV_CODEC_ID_H264) {
        auto profileLevelId = libav_utils::getDictValue(options_, "parameters");
        extractProfileLevelID(profileLevelId, encoderCtx);
#ifdef RING_ACCEL
#ifdef ENABLE_VIDEOTOOLBOX
        if (accel_) {
            maxBitrate = 2000 * std::atoi(libav_utils::getDictValue(options_, "max_rate"));
            bufSize = 2 * maxBitrate;
            crf = 20;
        }
#endif
        if (accel_)
            // limit the bitrate else it will easily go up to a few MiB/s
            encoderCtx->bit_rate = maxBitrate;
        else
#endif
        forcePresetX264(encoderCtx);
        // For H264 :
        // Streaming => VBV (constrained encoding) + CRF (Constant Rate Factor)
        if (crf == SystemCodecInfo::DEFAULT_NO_QUALITY)
            crf = 30; // good value for H264-720p@30
        JAMI_DBG("H264 encoder setup: crf=%u, maxrate=%u, bufsize=%u", crf, maxBitrate, bufSize);
        libav_utils::setDictValue(&options_, "crf", std::to_string(crf));
        av_opt_set_int(encoderCtx, "crf", crf, AV_OPT_SEARCH_CHILDREN);
        encoderCtx->rc_buffer_size = bufSize;
        encoderCtx->rc_max_rate = maxBitrate;
    } else if (systemCodecInfo.avcodecId == AV_CODEC_ID_HEVC) {
        forcePresetHEVC(encoderCtx);
        //force profile
        encoderCtx->profile = FF_PROFILE_HEVC_MAIN;
        encoderCtx->level = 0x01;
    } else if (systemCodecInfo.avcodecId == AV_CODEC_ID_VP8) {
        // For VP8 :
        // 1- if quality is set use it
        // bitrate need to be set. The target bitrate becomes the maximum allowed bitrate
        // 2- otherwise set rc_max_rate and rc_buffer_size
        // Using information given on this page:
        // http://www.webmproject.org/docs/encoder-parameters/
        av_opt_set(encoderCtx, "quality", "realtime", AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "error-resilient", 1, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "cpu-used", 7, AV_OPT_SEARCH_CHILDREN); // value obtained from testing
        av_opt_set_int(encoderCtx, "lag-in-frames", 0, AV_OPT_SEARCH_CHILDREN);
        // allow encoder to drop frames if buffers are full and
        // to undershoot target bitrate to lessen strain on resources
        av_opt_set_int(encoderCtx, "drop-frame", 25, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "undershoot-pct", 95, AV_OPT_SEARCH_CHILDREN);
        // don't set encoderCtx->gop_size: let libvpx decide when to insert a keyframe
        encoderCtx->slices = 2; // VP8E_SET_TOKEN_PARTITIONS
        encoderCtx->qmin = 4;
        encoderCtx->qmax = 56;
        encoderCtx->rc_buffer_size = maxBitrate;
        encoderCtx->bit_rate = maxBitrate;
        if (crf != SystemCodecInfo::DEFAULT_NO_QUALITY) {
            av_opt_set_int(encoderCtx, "crf", crf, AV_OPT_SEARCH_CHILDREN);
            JAMI_DBG("Using quality factor %d", crf);
        } else {
            JAMI_DBG("Using Max bitrate %d", maxBitrate);
        }
    } else if (systemCodecInfo.avcodecId == AV_CODEC_ID_VP9) {
        // Using information given on this page:
        // http://www.webmproject.org/docs/encoder-parameters/

        av_opt_set(encoderCtx->priv_data, "quality", "realtime", 0);
        av_opt_set_int(encoderCtx->priv_data, "error-resilient", 1, 0);
        /*av_opt_set_int(encoderCtx_->priv_data, "cpu-used", 3, 0);
        encoderCtx_->slices = 2; // VP8E_SET_TOKEN_PARTITIONS
        encoderCtx_->qmin = 4;
        encoderCtx_->qmax = 56;
        encoderCtx_->gop_size = 999999;
        */
    } else if (systemCodecInfo.avcodecId == AV_CODEC_ID_MPEG4) {
        // For MPEG4 :
        // No CRF avaiable.
        // Use CBR (set bitrate)
        encoderCtx->rc_buffer_size = maxBitrate;
        encoderCtx->bit_rate = encoderCtx->rc_min_rate = encoderCtx->rc_max_rate =  maxBitrate;
        JAMI_DBG("Using Max bitrate %d", maxBitrate);
    } else if (systemCodecInfo.avcodecId == AV_CODEC_ID_H263) {
        encoderCtx->bit_rate = encoderCtx->rc_max_rate =  maxBitrate;
        encoderCtx->rc_buffer_size = maxBitrate;
        JAMI_DBG("Using Max bitrate %d", maxBitrate);
    }

    // add video stream to outputformat context
    AVStream* stream = avformat_new_stream(outputCtx_, outputCodec);
    if (!stream)
        throw MediaEncoderException("Could not allocate stream");

    currentStreamIdx_ = stream->index;

    readConfig(&options_, encoderCtx);
    if (avcodec_open2(encoderCtx, outputCodec, &options_) < 0)
        throw MediaEncoderException("Could not open encoder");

#ifndef _WIN32
    avcodec_parameters_from_context(stream->codecpar, encoderCtx);
#else
    stream->codec = encoderCtx;
#endif
    // framerate is not copied from encoderCtx to stream
    stream->avg_frame_rate = encoderCtx->framerate;
#ifdef ENABLE_VIDEO
    if (systemCodecInfo.mediaType == MEDIA_VIDEO) {
        // allocate buffers for both scaled (pre-encoder) and encoded frames
        const int width = encoderCtx->width;
        const int height = encoderCtx->height;
        int format = encoderCtx->pix_fmt;
#ifdef RING_ACCEL
        if (accel_) {
            // hardware encoders require a specific pixel format
            auto desc = av_pix_fmt_desc_get(encoderCtx->pix_fmt);
            if (desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
                format = accel_->getSoftwareFormat();
        }
#endif
        scaledFrameBufferSize_ = videoFrameSize(format, width, height);
        if (scaledFrameBufferSize_ < 0)
            throw MediaEncoderException(("Could not compute buffer size: " + libav_utils::getError(scaledFrameBufferSize_)).c_str());
        else if (scaledFrameBufferSize_ <= AV_INPUT_BUFFER_MIN_SIZE)
            throw MediaEncoderException("buffer too small");

        scaledFrameBuffer_.reserve(scaledFrameBufferSize_);
        scaledFrame_.setFromMemory(scaledFrameBuffer_.data(), format, width, height);
    }
#endif // ENABLE_VIDEO

    return stream->index;
}

void
MediaEncoder::openIOContext()
{
    if (ioCtx_) {
        outputCtx_->pb = ioCtx_;
        outputCtx_->packet_size = outputCtx_->pb->buffer_size;
    } else {
        int ret = 0;
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 7, 100)
        const char* filename = outputCtx_->url;
#else
        const char* filename = outputCtx_->filename;
#endif
        if (!(outputCtx_->oformat->flags & AVFMT_NOFILE)) {
            if ((ret = avio_open(&outputCtx_->pb, filename, AVIO_FLAG_WRITE)) < 0) {
                std::stringstream ss;
                ss << "Could not open IO context for '" << filename << "': " << libav_utils::getError(ret);
                throw MediaEncoderException(ss.str().c_str());
            }
        }
    }
}

void
MediaEncoder::startIO()
{
    if (!outputCtx_->pb)
        openIOContext();
    if (avformat_write_header(outputCtx_, options_ ? &options_ : nullptr)) {
        JAMI_ERR("Could not write header for output file... check codec parameters");
        throw MediaEncoderException("Failed to write output file header");
    }

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 7, 100)
    av_dump_format(outputCtx_, 0, outputCtx_->url, 1);
#else
    av_dump_format(outputCtx_, 0, outputCtx_->filename, 1);
#endif
    initialized_ = true;
}

#ifdef ENABLE_VIDEO
int
MediaEncoder::encode(VideoFrame& input, bool is_keyframe, int64_t frame_number)
{
    if (!initialized_) {
        initStream(videoCodec_, input.pointer()->hw_frames_ctx);
        startIO();
    }

    AVFrame* frame;
#ifdef RING_ACCEL
    auto desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(input.format()));
    bool isHardware = desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL);
#ifdef ENABLE_VIDEOTOOLBOX
    //Videotoolbox handles frames allocations itself and do not need creating frame context manually.
    //Now videotoolbox supports only fully accelerated pipeline
    bool isVideotoolbox = static_cast<AVPixelFormat>(input.format()) == AV_PIX_FMT_VIDEOTOOLBOX;
    if (accel_ &&  isVideotoolbox) {
        // Fully accelerated pipeline, skip main memory
        frame = input.pointer();
    } else {
#else
    std::unique_ptr<VideoFrame> framePtr;
    if (accel_ && accel_->isLinked()) {
        // Fully accelerated pipeline, skip main memory
        frame = input.pointer();
    } else if (isHardware) {
        // Hardware decoded frame, transfer back to main memory
        // Transfer to GPU if we have a hardware encoder
        AVPixelFormat pix = (accel_ ? accel_->getSoftwareFormat() : AV_PIX_FMT_YUV420P);
        framePtr = video::HardwareAccel::transferToMainMemory(input, pix);
        if (accel_)
            framePtr = accel_->transfer(*framePtr);
        frame = framePtr->pointer();
    } else if (accel_) {
        // Software decoded frame with a hardware encoder, convert to accepted format first
        auto pix = accel_->getSoftwareFormat();
        if (input.format() != pix) {
            framePtr = scaler_.convertFormat(input, pix);
            framePtr = accel_->transfer(*framePtr);
        } else {
            framePtr = accel_->transfer(input);
        }
        frame = framePtr->pointer();
    } else {
#endif //ENABLE_VIDEOTOOLBOX
#endif
        libav_utils::fillWithBlack(scaledFrame_.pointer());
        scaler_.scale_with_aspect(input, scaledFrame_);
        frame = scaledFrame_.pointer();
#ifdef RING_ACCEL
    }
#endif

    AVCodecContext* enc = encoders_[currentStreamIdx_];
    frame->pts = frame_number;
    if (enc->framerate.num != enc->time_base.den || enc->framerate.den != enc->time_base.num)
        frame->pts /= (rational<int64_t>(enc->framerate) * rational<int64_t>(enc->time_base)).real<int64_t>();

    if (is_keyframe) {
        frame->pict_type = AV_PICTURE_TYPE_I;
        frame->key_frame = 1;
    } else {
        frame->pict_type = AV_PICTURE_TYPE_NONE;
        frame->key_frame = 0;
    }

    return encode(frame, currentStreamIdx_);
}
#endif // ENABLE_VIDEO

int
MediaEncoder::encodeAudio(AudioFrame& frame)
{
    if (!initialized_) {
        // Initialize on first video frame, or first audio frame if no video stream
        if (not videoOpts_.isValid())
            startIO();
        else
            return 0;
    }
    frame.pointer()->pts = sent_samples;
    sent_samples += frame.pointer()->nb_samples;
    encode(frame.pointer(), currentStreamIdx_);
    return 0;
}

int
MediaEncoder::encode(AVFrame* frame, int streamIdx)
{
    if (!initialized_ && frame) {
        // Initialize on first video frame, or first audio frame if no video stream
        bool isVideo = (frame->width > 0 && frame->height > 0);
        if (isVideo and videoOpts_.isValid()) {
            // Has video stream, so init with video frame
            streamIdx = initStream(videoCodec_, frame->hw_frames_ctx);
            startIO();
        } else if (!isVideo and !videoOpts_.isValid()) {
            // Only audio, for MediaRecorder, which doesn't use encodeAudio
            startIO();
        } else {
            return 0;
        }
    }
    int ret = 0;
    AVCodecContext* encoderCtx = encoders_[streamIdx];
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr; // packet data will be allocated by the encoder
    pkt.size = 0;

    ret = avcodec_send_frame(encoderCtx, frame);
    if (ret < 0)
        return -1;

    while (ret >= 0) {
        ret = avcodec_receive_packet(encoderCtx, &pkt);
        if (ret == AVERROR(EAGAIN))
            break;
        if (ret < 0 && ret != AVERROR_EOF) { // we still want to write our frame on EOF
            JAMI_ERR() << "Failed to encode frame: " << libav_utils::getError(ret);
            return ret;
        }

        if (pkt.size) {
            if (send(pkt, streamIdx))
                break;
        }
    }

    av_packet_unref(&pkt);
    return 0;
}

bool
MediaEncoder::send(AVPacket& pkt, int streamIdx)
{
    if (!initialized_) {
        streamIdx = initStream(videoCodec_, nullptr);
        startIO();
    }
    if (streamIdx < 0)
        streamIdx = currentStreamIdx_;
    if (streamIdx >= 0 and streamIdx < encoders_.size()) {
        auto encoderCtx = encoders_[streamIdx];
        pkt.stream_index = streamIdx;
        if (pkt.pts != AV_NOPTS_VALUE)
            pkt.pts = av_rescale_q(pkt.pts, encoderCtx->time_base,
                                outputCtx_->streams[streamIdx]->time_base);
        if (pkt.dts != AV_NOPTS_VALUE)
            pkt.dts = av_rescale_q(pkt.dts, encoderCtx->time_base,
                                outputCtx_->streams[streamIdx]->time_base);
    }
    // write the compressed frame
    auto ret = av_write_frame(outputCtx_, &pkt);
    if (ret < 0) {
        JAMI_ERR() << "av_write_frame failed: " << libav_utils::getError(ret);
    }
    return ret >= 0;
}

int
MediaEncoder::flush()
{
    int ret = 0;
    for (size_t i = 0; i < outputCtx_->nb_streams; ++i) {
        if (encode(nullptr, i) < 0) {
            JAMI_ERR() << "Could not flush stream #" << i;
            ret |= 1u << i; // provide a way for caller to know which streams failed
        }
    }
    return -ret;
}

std::string
MediaEncoder::print_sdp()
{
    /* theora sdp can be huge */
#ifndef _WIN32
    const auto sdp_size = outputCtx_->streams[currentStreamIdx_]->codecpar->extradata_size + 2048;
#else
    const auto sdp_size = outputCtx_->streams[currentStreamIdx_]->codec->extradata_size + 2048;
#endif
    std::string result;
    std::string sdp(sdp_size, '\0');
    av_sdp_create(&outputCtx_, 1, &(*sdp.begin()), sdp_size);
    std::istringstream iss(sdp);
    std::string line;
    while (std::getline(iss, line)) {
        /* strip windows line ending */
        line = line.substr(0, line.length() - 1);
        result += line + "\n";
    }
#ifdef DEBUG_SDP
    JAMI_DBG("Sending SDP:\n%s", result.c_str());
#endif
    return result;
}

AVCodecContext*
MediaEncoder::prepareEncoderContext(AVCodec* outputCodec, bool is_video)
{
    AVCodecContext* encoderCtx = avcodec_alloc_context3(outputCodec);

    auto encoderName = outputCodec->name; // guaranteed to be non null if AVCodec is not null

    encoderCtx->thread_count = std::min(std::thread::hardware_concurrency(), is_video ? 16u : 4u);
    JAMI_DBG("[%s] Using %d threads", encoderName, encoderCtx->thread_count);

    if (is_video) {
        // resolution must be a multiple of two
        encoderCtx->width = videoOpts_.width;
        encoderCtx->height = videoOpts_.height;

        // satisfy ffmpeg: denominator must be 16bit or less value
        // time base = 1/FPS
        av_reduce(&encoderCtx->framerate.num, &encoderCtx->framerate.den,
                  videoOpts_.frameRate.numerator(), videoOpts_.frameRate.denominator(),
                  (1U << 16) - 1);
        encoderCtx->time_base = av_inv_q(encoderCtx->framerate);

        // emit one intra frame every gop_size frames
        encoderCtx->max_b_frames = 0;
        encoderCtx->pix_fmt = AV_PIX_FMT_YUV420P;
#ifdef RING_ACCEL
        if (accel_)
            encoderCtx->pix_fmt = accel_->getFormat();
#endif

        // Fri Jul 22 11:37:59 EDT 2011:tmatth:XXX: DON'T set this, we want our
        // pps and sps to be sent in-band for RTP
        // This is to place global headers in extradata instead of every
        // keyframe.
        // encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    } else {
        encoderCtx->sample_fmt = AV_SAMPLE_FMT_S16;
        encoderCtx->sample_rate = std::max(8000, audioOpts_.sampleRate);
        encoderCtx->time_base = AVRational{1, encoderCtx->sample_rate};
        if (audioOpts_.nbChannels > 2 || audioOpts_.nbChannels < 1) {
            encoderCtx->channels = std::max(std::min(audioOpts_.nbChannels, 1), 2);
            JAMI_ERR() << "[" << encoderName << "] Clamping invalid channel count: "
                << audioOpts_.nbChannels << " -> " << encoderCtx->channels;
        } else {
            encoderCtx->channels = audioOpts_.nbChannels;
        }
        encoderCtx->channel_layout = av_get_default_channel_layout(encoderCtx->channels);
        if (audioOpts_.frameSize) {
            encoderCtx->frame_size = audioOpts_.frameSize;
            JAMI_DBG() << "[" << encoderName << "] Frame size " << encoderCtx->frame_size;
        } else {
            JAMI_WARN() << "[" << encoderName << "] Frame size not set";
        }
    }

    return encoderCtx;
}

void
MediaEncoder::forcePresetX264(AVCodecContext* encoderCtx)
{
    const char *speedPreset = "ultrafast";
    if (av_opt_set(encoderCtx, "preset", speedPreset, AV_OPT_SEARCH_CHILDREN))
        JAMI_WARN("Failed to set x264 preset '%s'", speedPreset);
    const char *tune = "zerolatency";
    if (av_opt_set(encoderCtx, "tune", tune, AV_OPT_SEARCH_CHILDREN))
        JAMI_WARN("Failed to set x264 tune '%s'", tune);
}

void MediaEncoder::forcePresetHEVC(AVCodecContext* encoderCtx)
{
    if (av_opt_set(encoderCtx->priv_data, "preset", "ultrafast", 0))
        JAMI_WARN("Failed to set x265 preset");
    /*
    char *tune = "psnr";
    if (av_opt_set(encoderCtx_->priv_data, "tune", tune, 0))
        RING_WARN("Failed to set x265 tune '%s'", tune);
    tune = "ssim";
    if (av_opt_set(encoderCtx_->priv_data, "tune", tune, 0))
        RING_WARN("Failed to set x265 tune '%s'", tune);
    */
    if (av_opt_set(encoderCtx->priv_data, "tune", "zerolatency", 0))
        JAMI_WARN("Failed to set x265 tune");
    if (av_opt_set(encoderCtx->priv_data, "crf", "40", 0))
        JAMI_WARN("Failed to set x265 crf");
    if (av_opt_set(encoderCtx->priv_data, "x265-params", "crf-min=30", 0))
        JAMI_WARN("Failed to set x265 crf");
    if (av_opt_set(encoderCtx->priv_data, "x265-params", "crf-max=50", 0))
        JAMI_WARN("Failed to set x265 crf");
    if (av_opt_set(encoderCtx->priv_data, "x265-params", "rd=0", 0))
        JAMI_WARN("Failed to set x265 rd");
    if (av_opt_set(encoderCtx->priv_data, "x265-params", "ctu=16", 0))
        JAMI_WARN("Failed to set x265 ctu");
    if (av_opt_set(encoderCtx->priv_data, "x265-params", "min-cu-size=16", 0))
        JAMI_WARN("Failed to set x265 ctu min");
    if (av_opt_set(encoderCtx->priv_data, "x265-params", "aq-strength=3", 0))
        JAMI_WARN("Failed to set x265 aq-strengh");
}

void
MediaEncoder::extractProfileLevelID(const std::string &parameters,
                                         AVCodecContext *ctx)
{
    // From RFC3984:
    // If no profile-level-id is present, the Baseline Profile without
    // additional constraints at Level 1 MUST be implied.
    ctx->profile = FF_PROFILE_H264_CONSTRAINED_BASELINE;
    ctx->level = 0x0d;
    // ctx->level = 0x0d; // => 13 aka 1.3
    if (parameters.empty())
        return;

    const std::string target("profile-level-id=");
    size_t needle = parameters.find(target);
    if (needle == std::string::npos)
        return;

    needle += target.length();
    const size_t id_length = 6; /* digits */
    const std::string profileLevelID(parameters.substr(needle, id_length));
    if (profileLevelID.length() != id_length)
        return;

    int result;
    std::stringstream ss;
    ss << profileLevelID;
    ss >> std::hex >> result;
    // profile-level id consists of three bytes
    const unsigned char profile_idc = result >> 16;             // 42xxxx -> 42
    const unsigned char profile_iop = ((result >> 8) & 0xff);   // xx80xx -> 80
    ctx->level = result & 0xff;                                 // xxxx0d -> 0d
    switch (profile_idc) {
        case FF_PROFILE_H264_BASELINE:
            // check constraint_set_1_flag
            if ((profile_iop & 0x40) >> 6)
                ctx->profile |= FF_PROFILE_H264_CONSTRAINED;
            break;
        case FF_PROFILE_H264_HIGH_10:
        case FF_PROFILE_H264_HIGH_422:
        case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
            // check constraint_set_3_flag
            if ((profile_iop & 0x10) >> 4)
                ctx->profile |= FF_PROFILE_H264_INTRA;
            break;
    }
    JAMI_DBG("Using profile %s (%x) and level %d", avcodec_profile_name(AV_CODEC_ID_H264, ctx->profile), ctx->profile, ctx->level);
}

#ifdef RING_ACCEL
void
MediaEncoder::enableAccel(bool enableAccel)
{
    enableAccel_ = enableAccel;
    emitSignal<DRing::ConfigurationSignal::HardwareEncodingChanged>(enableAccel_);
    if (!enableAccel_) {
        accel_.reset();
        for (auto enc : encoders_)
            enc->opaque = nullptr;
    }
}
#endif

unsigned
MediaEncoder::getStreamCount() const
{
    return (audioOpts_.isValid() + videoOpts_.isValid());
}

MediaStream
MediaEncoder::getStream(const std::string& name, int streamIdx) const
{
    // if streamIdx is negative, use currentStreamIdx_
    if (streamIdx < 0)
        streamIdx = currentStreamIdx_;
    // make sure streamIdx is valid
    if (getStreamCount() <= 0 || streamIdx < 0 || encoders_.size() < (unsigned)(streamIdx + 1))
        return {};
    auto enc = encoders_[streamIdx];
    // TODO set firstTimestamp
    auto ms = MediaStream(name, enc);
#ifdef RING_ACCEL
    if (accel_)
        ms.format = accel_->getSoftwareFormat();
#endif
    return ms;
}

void
MediaEncoder::readConfig(AVDictionary** dict, AVCodecContext* encoderCtx)
{
    std::string path = fileutils::get_config_dir() + DIR_SEPARATOR_STR + "encoder.json";
    std::string name = encoderCtx->codec->name;
    if (fileutils::isFile(path)) {
        try {
            Json::Value root;
            std::ifstream file(path);
            file >> root;
            if (!root.isObject()) {
                JAMI_ERR() << "Invalid encoder configuration: root is not an object";
                return;
            }
            const auto& config = root[name];
            if (config.isNull()) {
                JAMI_WARN() << "Encoder '" << name << "' not found in configuration file";
                return;
            }
            if (!config.isObject()) {
                JAMI_ERR() << "Invalid encoder configuration: '" << name << "' is not an object";
                return;
            }
            // If users want to change these, they should use the settings page.
            for (Json::Value::const_iterator it = config.begin(); it != config.end(); ++it) {
                Json::Value v = *it;
                if (!it.key().isConvertibleTo(Json::ValueType::stringValue)
                    || !v.isConvertibleTo(Json::ValueType::stringValue)) {
                    JAMI_ERR() << "Invalid configuration for '" << name << "'";
                    return;
                }
                const auto& key = it.key().asString();
                const auto& value = v.asString();
                // provides a way to override all AVCodecContext fields MediaEncoder sets
                if (key == "parameters") // Used by MediaEncoder for profile-level-id, ignore
                    continue;
                else if (value.empty())
                    libav_utils::setDictValue(dict, key, nullptr);
                else if (key == "profile")
                    encoderCtx->profile = v.asInt();
                else if (key == "level")
                    encoderCtx->level = v.asInt();
                else if (key == "bit_rate")
                    encoderCtx->bit_rate = v.asInt();
                else if (key == "rc_buffer_size")
                    encoderCtx->rc_buffer_size = v.asInt();
                else if (key == "rc_min_rate")
                    encoderCtx->rc_min_rate = v.asInt();
                else if (key == "rc_max_rate")
                    encoderCtx->rc_max_rate = v.asInt();
                else if (key == "qmin")
                    encoderCtx->qmin = v.asInt();
                else if (key == "qmax")
                    encoderCtx->qmax = v.asInt();
                else
                    libav_utils::setDictValue(dict, key, value);
            }
        } catch (const Json::Exception& e) {
            JAMI_ERR() << "Failed to load encoder configuration file: " << e.what();
        }
    }
}

} // namespace jami
