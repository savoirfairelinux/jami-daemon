/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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
        if (outputCtx_->priv_data && outputCtx_->pb)
            av_write_trailer(outputCtx_);
        if (fileIO_) {
            avio_close(outputCtx_->pb);
        }
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
        if (opts.bitrate)
            libav_utils::setDictValue(&options_, "max_rate", std::to_string(opts.bitrate));
    } else {
        audioOpts_ = opts;
    }
}

void
MediaEncoder::setOptions(const MediaDescription& args)
{
    int ret;
    if ((ret = av_opt_set_int(reinterpret_cast<void*>(outputCtx_),
        "payload_type", args.payload_type, AV_OPT_SEARCH_CHILDREN)) < 0)
        JAMI_ERR() << "Failed to set payload type: " << libav_utils::getError(ret);

    if (not args.parameters.empty())
        libav_utils::setDictValue(&options_, "parameters", args.parameters);

    auto_quality = args.auto_quality;
    linkableHW_ = args.linkableHW;
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
    std::lock_guard<std::mutex> lk(encMutex_);
    AVCodecContext* encoderCtx = nullptr;
    AVMediaType mediaType;

    if(systemCodecInfo.mediaType == MEDIA_VIDEO)
        mediaType = AVMEDIA_TYPE_VIDEO;
    else if(systemCodecInfo.mediaType == MEDIA_AUDIO)
        mediaType = AVMEDIA_TYPE_AUDIO;


    // add video stream to outputformat context
    AVStream* stream = avformat_new_stream(outputCtx_, outputCodec_);
    if (!stream)
        throw MediaEncoderException("Could not allocate stream");

    currentStreamIdx_ = stream->index;
#ifdef RING_ACCEL
    // Get compatible list of Hardware API
    if (enableAccel_ && mediaType == AVMEDIA_TYPE_VIDEO) {
        auto APIs = video::HardwareAccel::getCompatibleAccel(static_cast<AVCodecID>(systemCodecInfo.avcodecId),
                videoOpts_.width, videoOpts_.height, CODEC_ENCODER);
        for (const auto& it : APIs) {
            accel_ = std::make_unique<video::HardwareAccel>(it);    // save accel
            // Init codec need accel_ to init encoderCtx accelerated
            encoderCtx = initCodec(mediaType, static_cast<AVCodecID>(systemCodecInfo.avcodecId), SystemCodecInfo::DEFAULT_VIDEO_BITRATE);
            encoderCtx->opaque = accel_.get();
            // Check if pixel format from encoder match pixel format from decoder frame context
            // if it mismatch, it means that we are using two different hardware API (nvenc and vaapi for example)
            // in this case we don't want link the APIs
            if (framesCtx) {
                auto hw = reinterpret_cast<AVHWFramesContext*>(framesCtx->data);
                if (encoderCtx->pix_fmt != hw->format)
                    linkableHW_ = false;
            }
            auto ret = accel_->initAPI(linkableHW_, framesCtx);
            if (ret < 0) {
                accel_.reset();
                encoderCtx = nullptr;
                continue;
            }
            accel_->setDetails(encoderCtx);
            if (avcodec_open2(encoderCtx, outputCodec_, &options_) < 0) {
                // Failed to open codec
                JAMI_WARN("Fail to open hardware encoder %s with %s ", avcodec_get_name(static_cast<AVCodecID>(systemCodecInfo.avcodecId)), it.getName().c_str());
                avcodec_free_context(&encoderCtx);
                encoderCtx = nullptr;
                accel_ = nullptr;
                continue;
            } else {
                // Succeed to open codec
                JAMI_WARN("Using hardware encoding for %s with %s ", avcodec_get_name(static_cast<AVCodecID>(systemCodecInfo.avcodecId)), it.getName().c_str());
                encoders_.push_back(encoderCtx);
                break;
            }
        }
    }
#endif

    if (!encoderCtx) {
        JAMI_WARN("Not using hardware encoding for %s", avcodec_get_name(static_cast<AVCodecID>(systemCodecInfo.avcodecId)));
        encoderCtx = initCodec(mediaType, static_cast<AVCodecID>(systemCodecInfo.avcodecId), SystemCodecInfo::DEFAULT_VIDEO_BITRATE);
        readConfig(encoderCtx);
        encoders_.push_back(encoderCtx);
        if (avcodec_open2(encoderCtx, outputCodec_, &options_) < 0)
            throw MediaEncoderException("Could not open encoder");
    }

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
            fileIO_ = true;
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
    if (accel_ && accel_->isLinked() && isHardware) {
        // Fully accelerated pipeline, skip main memory
        // We have to check if the frame is hardware even if
        // we are using linked HW encoder and decoder because after
        // conference mixing the frame become software (prevent crashes)
        frame = input.pointer();
    } else if (isHardware) {
        // Hardware decoded frame, transfer back to main memory
        // Transfer to GPU if we have a hardware encoder
        // Hardware decoders decode to NV12, but Jami's supported software encoders want YUV420P
        AVPixelFormat pix = (accel_ ? accel_->getSoftwareFormat() : AV_PIX_FMT_NV12);
        framePtr = video::HardwareAccel::transferToMainMemory(input, pix);
        if (!accel_)
            framePtr = scaler_.convertFormat(*framePtr, AV_PIX_FMT_YUV420P);
        else
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
    if (streamIdx >= 0 and static_cast<size_t>(streamIdx) < encoders_.size()) {
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
// Keep YUV format for macOS
#if !(defined(__APPLE__) && !TARGET_OS_IOS)
        if (accel_)
            encoderCtx->pix_fmt = accel_->getFormat();
#endif
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
MediaEncoder::forcePresetX2645(AVCodecContext* encoderCtx)
{
    if(accel_ && accel_->getName() == "nvenc") {
        if (av_opt_set(encoderCtx, "preset", "fast", AV_OPT_SEARCH_CHILDREN))
            JAMI_WARN("Failed to set preset to 'fast'");
        if (av_opt_set(encoderCtx, "level", "auto", AV_OPT_SEARCH_CHILDREN))
            JAMI_WARN("Failed to set level to 'auto'");
        if (av_opt_set_int(encoderCtx, "zerolatency", 1, AV_OPT_SEARCH_CHILDREN))
            JAMI_WARN("Failed to set zerolatency to '1'");
    }
    else {
#if (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        const char *speedPreset = "ultrafast";
#else
        const char *speedPreset = "veryfast";
#endif
        if (av_opt_set(encoderCtx, "preset", speedPreset, AV_OPT_SEARCH_CHILDREN))
            JAMI_WARN("Failed to set preset '%s'", speedPreset);
        const char *tune = "zerolatency";
        if (av_opt_set(encoderCtx, "tune", tune, AV_OPT_SEARCH_CHILDREN))
            JAMI_WARN("Failed to set tune '%s'", tune);
    }
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

AVCodecContext*
MediaEncoder::initCodec(AVMediaType mediaType, AVCodecID avcodecId, uint64_t br)
{
    outputCodec_ = nullptr;
#ifdef RING_ACCEL
    if (mediaType == AVMEDIA_TYPE_VIDEO) {
        if (enableAccel_) {
            if (accel_) {
                outputCodec_ = avcodec_find_encoder_by_name(accel_->getCodecName().c_str());
            }
        } else {
            JAMI_WARN() << "Hardware encoding disabled";
        }
    }
#endif

    if (!outputCodec_) {
        /* find the video encoder */
        if (avcodecId == AV_CODEC_ID_H263)
            // For H263 encoding, we force the use of AV_CODEC_ID_H263P (H263-1998)
            // H263-1998 can manage all frame sizes while H263 don't
            // AV_CODEC_ID_H263 decoder will be used for decoding
            outputCodec_ = avcodec_find_encoder(AV_CODEC_ID_H263P);
        else
            outputCodec_ = avcodec_find_encoder(static_cast<AVCodecID>(avcodecId));
        if (!outputCodec_) {
            throw MediaEncoderException("No output encoder");
        }
    }

    AVCodecContext* encoderCtx = prepareEncoderContext(outputCodec_, mediaType == AVMEDIA_TYPE_VIDEO);

    // Only clamp video bitrate
    if (mediaType == AVMEDIA_TYPE_VIDEO && br > 0) {
        if (br < SystemCodecInfo::DEFAULT_MIN_BITRATE) {
            JAMI_WARN("Requested bitrate %lu too low, setting to %u",
                br, SystemCodecInfo::DEFAULT_MIN_BITRATE);
            br = SystemCodecInfo::DEFAULT_MIN_BITRATE;
        } else if (br > SystemCodecInfo::DEFAULT_MAX_BITRATE) {
            JAMI_WARN("Requested bitrate %lu too high, setting to %u",
                br, SystemCodecInfo::DEFAULT_MAX_BITRATE);
            br = SystemCodecInfo::DEFAULT_MAX_BITRATE;
        }
    }

    /* let x264 preset override our encoder settings */
    if (avcodecId == AV_CODEC_ID_H264) {
        auto profileLevelId = libav_utils::getDictValue(options_, "parameters");
        extractProfileLevelID(profileLevelId, encoderCtx);
        forcePresetX2645(encoderCtx);
        initH264(encoderCtx, br);
    } else if (avcodecId == AV_CODEC_ID_HEVC) {
        encoderCtx->profile = FF_PROFILE_HEVC_MAIN;
        forcePresetX2645(encoderCtx);
        initH265(encoderCtx, br);
    } else if (avcodecId == AV_CODEC_ID_VP8) {
        initVP8(encoderCtx, br);
    } else if (avcodecId == AV_CODEC_ID_MPEG4) {
        initMPEG4(encoderCtx, br);
    } else if (avcodecId == AV_CODEC_ID_H263) {
        initH263(encoderCtx, br);
    }
    return encoderCtx;
}

void
MediaEncoder::setBitrate(uint64_t br)
{
    std::lock_guard<std::mutex> lk(encMutex_);
    AVCodecContext* encoderCtx = getCurrentVideoAVCtx();
    if(not encoderCtx)
        return;

    AVMediaType codecType = encoderCtx->codec_type;
    AVCodecID codecId = encoderCtx->codec_id;

    // No need to restart encoder for h264, h263 and MPEG4
    // Change parameters on the fly
    if(codecId == AV_CODEC_ID_H264)
        initH264(encoderCtx, br);
    if(codecId == AV_CODEC_ID_HEVC)
        initH265(encoderCtx, br);
    else if(codecId == AV_CODEC_ID_H263P)
        initH263(encoderCtx, br);
    else if(codecId == AV_CODEC_ID_MPEG4)
        initMPEG4(encoderCtx, br);
    else {
        // restart encoder on runtime doesn't work for VP8
        // stopEncoder();
        // encoderCtx = initCodec(codecType, codecId, br);
        // if (avcodec_open2(encoderCtx, outputCodec_, &options_) < 0)
        //     throw MediaEncoderException("Could not open encoder");
    }
}

void
MediaEncoder::initH264(AVCodecContext* encoderCtx, uint64_t br)
{
    // If auto quality disabled use CRF mode
    if(not auto_quality) {
        uint64_t maxBitrate = 1000 * br;
        uint8_t crf = (uint8_t) std::round(LOGREG_PARAM_A + log(pow(maxBitrate, LOGREG_PARAM_B)));     // CRF = A + B*ln(maxBitrate)
        uint64_t bufSize = maxBitrate * 2;

        av_opt_set_int(encoderCtx, "crf", crf, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "b", maxBitrate, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "maxrate", maxBitrate, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "minrate", -1, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "bufsize", bufSize, AV_OPT_SEARCH_CHILDREN);
        JAMI_DBG("H264 encoder setup: crf=%u, maxrate=%lu, bufsize=%lu", crf, maxBitrate, bufSize);
    }
    // If auto quality enabled use CRB mode
    else {
        av_opt_set_int(encoderCtx, "b", br * 1000, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "maxrate", br * 1000, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "minrate", br * 1000, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "bufsize", br * 500, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "crf", -1, AV_OPT_SEARCH_CHILDREN);

        JAMI_DBG("H264 encoder setup cbr: bitrate=%lu kbit/s", br);
    }
}

void
MediaEncoder::initH265(AVCodecContext* encoderCtx, uint64_t br)
{
    // If auto quality disabled use CRF mode
    if(not auto_quality) {
        uint64_t maxBitrate = 1000 * br;
        // H265 use 50% less bitrate compared to H264 (half bitrate is equivalent to a change 6 for CRF)
        // https://slhck.info/video/2017/02/24/crf-guide.html
        uint8_t crf = (uint8_t) std::round(LOGREG_PARAM_A + log(pow(maxBitrate, LOGREG_PARAM_B)) - 6);     // CRF = A + B*ln(maxBitrate)
        uint64_t bufSize = maxBitrate * 2;

        av_opt_set_int(encoderCtx, "crf", crf, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "b", maxBitrate, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "maxrate", maxBitrate, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "minrate", -1, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "bufsize", bufSize, AV_OPT_SEARCH_CHILDREN);
        JAMI_DBG("H265 encoder setup: crf=%u, maxrate=%lu, bufsize=%lu", crf, maxBitrate, bufSize);
    }
    else {
        av_opt_set_int(encoderCtx, "b", br * 1000, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "maxrate", br * 1000, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "minrate", br * 1000, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "bufsize", br * 500, AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(encoderCtx, "crf", -1, AV_OPT_SEARCH_CHILDREN);

        JAMI_DBG("H265 encoder setup cbr: bitrate=%lu kbit/s", br);
    }

}

void
MediaEncoder::initVP8(AVCodecContext* encoderCtx, uint64_t br)
{
    // 1- if quality is set use it
    // bitrate need to be set. The target bitrate becomes the maximum allowed bitrate
    // 2- otherwise set rc_max_rate and rc_buffer_size
    // Using information given on this page:
    // http://www.webmproject.org/docs/encoder-parameters/
    uint64_t maxBitrate = 1000 * br;
    uint8_t crf = (uint8_t) std::round(LOGREG_PARAM_A + log(pow(maxBitrate, LOGREG_PARAM_B)));     // CRF = A + B*ln(maxBitrate)
    uint64_t bufSize = 2 * maxBitrate;

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
    crf = std::min(encoderCtx->qmax, std::max((int)crf, encoderCtx->qmin));
    av_opt_set_int(encoderCtx, "crf", crf, AV_OPT_SEARCH_CHILDREN);
    encoderCtx->rc_buffer_size = bufSize;
    encoderCtx->rc_max_rate = maxBitrate;
    JAMI_DBG("VP8 encoder setup: crf=%u, maxrate=%lu, bufsize=%lu", crf, maxBitrate, maxBitrate);
}

void
MediaEncoder::initMPEG4(AVCodecContext* encoderCtx, uint64_t br)
{
    uint64_t maxBitrate = 1000 * br;
    uint64_t bufSize = 2 * maxBitrate;

    // Use CBR (set bitrate)
    encoderCtx->rc_buffer_size = bufSize;
    encoderCtx->bit_rate = encoderCtx->rc_min_rate = encoderCtx->rc_max_rate = maxBitrate;
    JAMI_DBG("MPEG4 encoder setup: maxrate=%lu, bufsize=%lu", maxBitrate, bufSize);
}

void
MediaEncoder::initH263(AVCodecContext* encoderCtx, uint64_t br)
{
    uint64_t maxBitrate = 1000 * br;
    uint64_t bufSize = 2 * maxBitrate;

    // Use CBR (set bitrate)
    encoderCtx->rc_buffer_size = bufSize;
    encoderCtx->bit_rate = encoderCtx->rc_min_rate = encoderCtx->rc_max_rate = maxBitrate;
    JAMI_DBG("H263 encoder setup: maxrate=%lu, bufsize=%lu", maxBitrate, bufSize);
}

AVCodecContext*
MediaEncoder::getCurrentVideoAVCtx()
{
    for (auto it : encoders_) {
        if (it->codec_type == AVMEDIA_TYPE_VIDEO)
            return it;
    }
    return nullptr;
}

void
MediaEncoder::stopEncoder()
{
    flush();
    for (auto it = encoders_.begin(); it != encoders_.end(); it++) {
        if ((*it)->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            encoders_.erase(it);
            break;
        }
    }
    AVCodecContext* encoderCtx = getCurrentVideoAVCtx();
    avcodec_close(encoderCtx);
    avcodec_free_context(&encoderCtx);
    av_free(encoderCtx);
}

void
MediaEncoder::readConfig(AVCodecContext* encoderCtx)
{
    std::string path = fileutils::get_config_dir() + DIR_SEPARATOR_STR + "encoder.json";
    std::string name = encoderCtx->codec->name;
    if (fileutils::isFile(path)) {
        JAMI_WARN("encoder.json file found, default settings will be erased");
        try {
            Json::Value root;
            std::ifstream file = fileutils::ifstream(path);
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
            for (Json::Value::const_iterator it = config.begin(); it != config.end(); ++it) {
                Json::Value v = *it;
                if (!it.key().isConvertibleTo(Json::ValueType::stringValue)
                    || !v.isConvertibleTo(Json::ValueType::stringValue)) {
                    JAMI_ERR() << "Invalid configuration for '" << name << "'";
                    return;
                }
                const auto& key = it.key().asString();
                const auto& value = v.asString();
                int ret = av_opt_set(reinterpret_cast<void*>(encoderCtx),
                                     key.c_str(), value.c_str(), AV_OPT_SEARCH_CHILDREN);
                if (ret < 0) {
                    JAMI_ERR() << "Failed to set option " << key << " in " << name << " context: "
                        << libav_utils::getError(ret) << "\n";
                }
            }
        } catch (const Json::Exception& e) {
            JAMI_ERR() << "Failed to load encoder configuration file: " << e.what();
        }
    }
}

std::string
MediaEncoder::testH265Accel()
{
#ifdef RING_ACCEL
    if (jami::Manager::instance().videoPreferences.getEncodingAccelerated()) {
        // Get compatible list of Hardware API
        auto APIs = video::HardwareAccel::getCompatibleAccel(AV_CODEC_ID_H265,
                1280, 720, CODEC_ENCODER);

        std::unique_ptr<video::HardwareAccel> accel;

        for (const auto& it : APIs) {
            accel = std::make_unique<video::HardwareAccel>(it);    // save accel
            // Init codec need accel to init encoderCtx accelerated
            auto outputCodec = avcodec_find_encoder_by_name(accel->getCodecName().c_str());

            AVCodecContext* encoderCtx = avcodec_alloc_context3(outputCodec);
            encoderCtx->thread_count = std::min(std::thread::hardware_concurrency(), 16u);
            encoderCtx->width = 1280;
            encoderCtx->height = 720;
            AVRational framerate;
            framerate.num = 30;
            framerate.den = 1;
            encoderCtx->time_base = av_inv_q(framerate);
            encoderCtx->pix_fmt = accel->getFormat();
            encoderCtx->profile = FF_PROFILE_HEVC_MAIN;
            encoderCtx->opaque = accel.get();

            auto br = SystemCodecInfo::DEFAULT_VIDEO_BITRATE;
            av_opt_set_int(encoderCtx, "b", br * 1000, AV_OPT_SEARCH_CHILDREN);
            av_opt_set_int(encoderCtx, "maxrate", br * 1000, AV_OPT_SEARCH_CHILDREN);
            av_opt_set_int(encoderCtx, "minrate", br * 1000, AV_OPT_SEARCH_CHILDREN);
            av_opt_set_int(encoderCtx, "bufsize", br * 500, AV_OPT_SEARCH_CHILDREN);
            av_opt_set_int(encoderCtx, "crf", -1, AV_OPT_SEARCH_CHILDREN);

            auto ret = accel->initAPI(false, nullptr);
            if (ret < 0) {
                accel.reset();;
                encoderCtx = nullptr;
                continue;
            }
            accel->setDetails(encoderCtx);
            if (avcodec_open2(encoderCtx, outputCodec, nullptr) < 0) {
                // Failed to open codec
                JAMI_WARN("Fail to open hardware encoder H265 with %s ", it.getName().c_str());
                avcodec_free_context(&encoderCtx);
                encoderCtx = nullptr;
                accel = nullptr;
                continue;
            } else {
                // Succeed to open codec
                avcodec_free_context(&encoderCtx);
                encoderCtx = nullptr;
                accel = nullptr;
                return it.getName();
            }
        }
    }
#endif
    return "";
}

} // namespace jami
