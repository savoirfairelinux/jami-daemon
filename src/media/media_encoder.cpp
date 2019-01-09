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
#include "media_io_handle.h"

#include "audio/audiobuffer.h"
#include "string_utils.h"
#include "logger.h"

extern "C" {
#include <libavutil/parseutils.h>
}

#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread> // hardware_concurrency

// Define following line if you need to debug libav SDP
//#define DEBUG_SDP 1

namespace ring {

MediaEncoder::MediaEncoder()
    : outputCtx_(avformat_alloc_context())
{}

MediaEncoder::~MediaEncoder()
{
    if (outputCtx_) {
        if (outputCtx_->priv_data)
            av_write_trailer(outputCtx_);

        for (auto encoderCtx : encoders_)
            if (encoderCtx) {
#ifndef _MSC_VER
                avcodec_free_context(&encoderCtx);
#else
                avcodec_close(encoderCtx);
#endif
            }

        avformat_free_context(outputCtx_);
    }

    av_dict_free(&options_);
}

void MediaEncoder::setDeviceOptions(const DeviceParams& args)
{
    device_ = args;
    // Make sure width and height are even (required by x264)
    // This is especially for image/gif streaming, as video files and cameras usually have even resolutions
    device_.width -= device_.width % 2;
    device_.height -= device_.height % 2;
    if (device_.width)
        libav_utils::setDictValue(&options_, "width", ring::to_string(device_.width));
    if (device_.height)
        libav_utils::setDictValue(&options_, "height", ring::to_string(device_.height));
    if (not device_.framerate)
        device_.framerate = 30;
    libav_utils::setDictValue(&options_, "framerate", ring::to_string(device_.framerate.real()));
}

void MediaEncoder::setOptions(const MediaDescription& args)
{
    codec_ = args.codec;

    libav_utils::setDictValue(&options_, "payload_type", ring::to_string(args.payload_type));
    libav_utils::setDictValue(&options_, "max_rate", ring::to_string(args.codec->bitrate));
    libav_utils::setDictValue(&options_, "crf", ring::to_string(args.codec->quality));

    if (args.codec->systemCodecInfo.mediaType == MEDIA_AUDIO) {
        auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(args.codec);
        if (accountAudioCodec->audioformat.sample_rate)
            libav_utils::setDictValue(&options_, "sample_rate",
                        ring::to_string(accountAudioCodec->audioformat.sample_rate));

        if (accountAudioCodec->audioformat.nb_channels)
            libav_utils::setDictValue(&options_, "channels",
                        ring::to_string(accountAudioCodec->audioformat.nb_channels));

        if (accountAudioCodec->audioformat.sample_rate && accountAudioCodec->audioformat.nb_channels)
            libav_utils::setDictValue(&options_, "frame_size",
                        ring::to_string(static_cast<unsigned>(0.02 * accountAudioCodec->audioformat.sample_rate)));
    }

    if (not args.parameters.empty())
        libav_utils::setDictValue(&options_, "parameters", args.parameters);
}

void
MediaEncoder::setInitSeqVal(uint16_t seqVal)
{
    //only set not default value (!=0)
    if (seqVal != 0)
        libav_utils::setDictValue(&options_, "seq", ring::to_string(seqVal));
}

uint16_t
MediaEncoder::getLastSeqValue()
{
    int64_t  retVal;
    auto ret = av_opt_get_int(outputCtx_->priv_data, "seq", AV_OPT_SEARCH_CHILDREN, &retVal);
    if (ret == 0)
        return (uint16_t) retVal;
    else
        return 0;
}

std::string
MediaEncoder::getEncoderName() const
{
    return encoders_[currentStreamIdx_]->codec->name;
}

void
MediaEncoder::openLiveOutput(const std::string& filename,
                             const ring::MediaDescription& args)
{
    setOptions(args);
    AVOutputFormat *oformat = av_guess_format("rtp", filename.c_str(), nullptr);

    if (!oformat) {
        RING_ERR("Unable to find a suitable output format for %s", filename.c_str());
        throw MediaEncoderException("No output format");
    }

    outputCtx_->oformat = oformat;
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 7, 100)
    // c_str guarantees null termination
    outputCtx_->url = av_strdup(filename.c_str()); // must be compatible with av_free
#else
    strncpy(outputCtx_->filename, filename.c_str(), sizeof(outputCtx_->filename));
    // in case our filename is longer than the space reserved for AVFormatContext.filename
    outputCtx_->filename[sizeof(outputCtx_->filename) - 1] = '\0';
#endif

    addStream(args.codec->systemCodecInfo, args.parameters);
}

void
MediaEncoder::openFileOutput(const std::string& filename, std::map<std::string, std::string> options)
{
    avformat_free_context(outputCtx_);
    avformat_alloc_output_context2(&outputCtx_, nullptr, nullptr, filename.c_str());

    if (!options["title"].empty())
        libav_utils::setDictValue(&outputCtx_->metadata, "title", options["title"]);
    if (!options["description"].empty())
        libav_utils::setDictValue(&outputCtx_->metadata, "description", options["description"]);

    auto bitrate = SystemCodecInfo::DEFAULT_MAX_BITRATE;
    auto quality = SystemCodecInfo::DEFAULT_CODEC_QUALITY;
    // ensure all options retrieved later on are in options_ (insert does nothing if key exists)
    options.insert({"max_rate", ring::to_string(bitrate)});
    options.insert({"crf", ring::to_string(quality)});
    options.insert({"sample_rate", "8000"});
    options.insert({"channels", "2"});
    int sampleRate = atoi(options["sample_rate"].c_str());
    options.insert({"frame_size", ring::to_string(static_cast<unsigned>(0.02*sampleRate))});
    options.insert({"width", "320"});
    options.insert({"height", "240"});
    options.insert({"framerate", "30.00"});
    for (const auto& it : options)
        libav_utils::setDictValue(&options_, it.first, it.second);
    // for a file output, addStream is done by the caller, as there may be multiple streams
}

int
MediaEncoder::addStream(const SystemCodecInfo& systemCodecInfo, std::string parameters)
{
    AVCodec* outputCodec = nullptr;
    AVCodecContext* encoderCtx = nullptr;
    /* find the video encoder */
    if (systemCodecInfo.avcodecId == AV_CODEC_ID_H263)
        // For H263 encoding, we force the use of AV_CODEC_ID_H263P (H263-1998)
        // H263-1998 can manage all frame sizes while H263 don't
        // AV_CODEC_ID_H263 decoder will be used for decoding
        outputCodec = avcodec_find_encoder(AV_CODEC_ID_H263P);
    else
        outputCodec = avcodec_find_encoder(static_cast<AVCodecID>(systemCodecInfo.avcodecId));
    if (!outputCodec) {
        RING_ERR("Encoder \"%s\" not found!", systemCodecInfo.name.c_str());
        throw MediaEncoderException("No output encoder");
    }

    encoderCtx = prepareEncoderContext(outputCodec, systemCodecInfo.mediaType == MEDIA_VIDEO);
    encoders_.push_back(encoderCtx);
    auto maxBitrate = 1000 * std::atoi(libav_utils::getDictValue(options_, "max_rate"));
    auto bufSize = 2 * maxBitrate; // as recommended (TODO: make it customizable)
    auto crf = std::atoi(libav_utils::getDictValue(options_, "crf"));

    /* let x264 preset override our encoder settings */
    if (systemCodecInfo.avcodecId == AV_CODEC_ID_H264) {
        extractProfileLevelID(parameters, encoderCtx);
        forcePresetX264(encoderCtx);
        // For H264 :
        // Streaming => VBV (constrained encoding) + CRF (Constant Rate Factor)
        if (crf == SystemCodecInfo::DEFAULT_NO_QUALITY)
            crf = 30; // good value for H264-720p@30
        RING_DBG("H264 encoder setup: crf=%u, maxrate=%u, bufsize=%u", crf, maxBitrate, bufSize);

        av_opt_set_int(encoderCtx->priv_data, "crf", crf, 0);
        encoderCtx->rc_buffer_size = bufSize;
        encoderCtx->rc_max_rate = maxBitrate;
    } else if (systemCodecInfo.avcodecId == AV_CODEC_ID_VP8) {
        // For VP8 :
        // 1- if quality is set use it
        // bitrate need to be set. The target bitrate becomes the maximum allowed bitrate
        // 2- otherwise set rc_max_rate and rc_buffer_size
        // Using information given on this page:
        // http://www.webmproject.org/docs/encoder-parameters/
        av_opt_set(encoderCtx->priv_data, "quality", "realtime", 0);
        av_opt_set_int(encoderCtx->priv_data, "error-resilient", 1, 0);
        av_opt_set_int(encoderCtx->priv_data, "cpu-used", 7, 0); // value obtained from testing
        av_opt_set_int(encoderCtx->priv_data, "lag-in-frames", 0, 0);
        // allow encoder to drop frames if buffers are full and
        // to undershoot target bitrate to lessen strain on resources
        av_opt_set_int(encoderCtx->priv_data, "drop-frame", 25, 0);
        av_opt_set_int(encoderCtx->priv_data, "undershoot-pct", 95, 0);
        // don't set encoderCtx->gop_size: let libvpx decide when to insert a keyframe
        encoderCtx->slices = 2; // VP8E_SET_TOKEN_PARTITIONS
        encoderCtx->qmin = 4;
        encoderCtx->qmax = 56;
        encoderCtx->rc_buffer_size = maxBitrate;
        encoderCtx->bit_rate = maxBitrate;
        if (crf != SystemCodecInfo::DEFAULT_NO_QUALITY) {
            av_opt_set_int(encoderCtx->priv_data, "crf", crf, 0);
            RING_DBG("Using quality factor %d", crf);
        } else {
            RING_DBG("Using Max bitrate %d", maxBitrate);
        }
    } else if (systemCodecInfo.avcodecId == AV_CODEC_ID_MPEG4) {
        // For MPEG4 :
        // No CRF avaiable.
        // Use CBR (set bitrate)
        encoderCtx->rc_buffer_size = maxBitrate;
        encoderCtx->bit_rate = encoderCtx->rc_min_rate = encoderCtx->rc_max_rate =  maxBitrate;
        RING_DBG("Using Max bitrate %d", maxBitrate);
    } else if (systemCodecInfo.avcodecId == AV_CODEC_ID_H263) {
        encoderCtx->bit_rate = encoderCtx->rc_max_rate =  maxBitrate;
        encoderCtx->rc_buffer_size = maxBitrate;
        RING_DBG("Using Max bitrate %d", maxBitrate);
    }

    // add video stream to outputformat context
    AVStream* stream = avformat_new_stream(outputCtx_, outputCodec);
    if (!stream)
        throw MediaEncoderException("Could not allocate stream");

    currentStreamIdx_ = stream->index;

    if (avcodec_open2(encoderCtx, outputCodec, nullptr) < 0)
        throw MediaEncoderException("Could not open encoder");

#ifndef _WIN32
    avcodec_parameters_from_context(stream->codecpar, encoderCtx);
#else
    stream->codec = encoderCtx;
#endif
    // framerate is not copied from encoderCtx to stream
    stream->avg_frame_rate = encoderCtx->framerate;
#ifdef RING_VIDEO
    if (systemCodecInfo.mediaType == MEDIA_VIDEO) {
        // allocate buffers for both scaled (pre-encoder) and encoded frames
        const int width = encoderCtx->width;
        const int height = encoderCtx->height;
        const int format = encoderCtx->pix_fmt;
        scaledFrameBufferSize_ = videoFrameSize(format, width, height);
        if (scaledFrameBufferSize_ < 0)
            throw MediaEncoderException(("Could not compute buffer size: " + libav_utils::getError(scaledFrameBufferSize_)).c_str());
        else if (scaledFrameBufferSize_ <= AV_INPUT_BUFFER_MIN_SIZE)
            throw MediaEncoderException("buffer too small");

        scaledFrameBuffer_.reserve(scaledFrameBufferSize_);
        scaledFrame_.setFromMemory(scaledFrameBuffer_.data(), format, width, height);
    }
#endif // RING_VIDEO

    return stream->index;
}

void MediaEncoder::setInterruptCallback(int (*cb)(void*), void *opaque)
{
    if (cb) {
        outputCtx_->interrupt_callback.callback = cb;
        outputCtx_->interrupt_callback.opaque = opaque;
    } else {
        outputCtx_->interrupt_callback.callback = 0;
    }
}

void MediaEncoder::setIOContext(const std::unique_ptr<MediaIOHandle> &ioctx)
{
    if (ioctx) {
        outputCtx_->pb = ioctx->getContext();
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
                ss << "Could not set IO context for '" << filename << "': " << libav_utils::getError(ret);
                throw MediaEncoderException(ss.str().c_str());
            }
        }
    }
}

void
MediaEncoder::startIO()
{
    if (avformat_write_header(outputCtx_, options_ ? &options_ : nullptr)) {
        RING_ERR("Could not write header for output file... check codec parameters");
        throw MediaEncoderException("Failed to write output file header");
    }

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 7, 100)
    av_dump_format(outputCtx_, 0, outputCtx_->url, 1);
#else
    av_dump_format(outputCtx_, 0, outputCtx_->filename, 1);
#endif
}

#ifdef RING_VIDEO
int
MediaEncoder::encode(VideoFrame& input, bool is_keyframe,
                     int64_t frame_number)
{
    /* Prepare a frame suitable to our encoder frame format,
     * keeping also the input aspect ratio.
     */
    libav_utils::fillWithBlack(scaledFrame_.pointer());

    scaler_.scale_with_aspect(input, scaledFrame_);

    auto frame = scaledFrame_.pointer();
    AVCodecContext* enc = encoders_[currentStreamIdx_];
    // ideally, time base is the inverse of framerate, but this may not always be the case
    if (enc->framerate.num == enc->time_base.den && enc->framerate.den == enc->time_base.num)
        frame->pts = frame_number;
    else
        frame->pts = (frame_number / (rational<int64_t>(enc->framerate) * rational<int64_t>(enc->time_base))).real<int64_t>();

    if (is_keyframe) {
        frame->pict_type = AV_PICTURE_TYPE_I;
        frame->key_frame = 1;
    } else {
        frame->pict_type = AV_PICTURE_TYPE_NONE;
        frame->key_frame = 0;
    }

    return encode(frame, currentStreamIdx_);
}
#endif // RING_VIDEO

int MediaEncoder::encodeAudio(AudioFrame& frame)
{
    frame.pointer()->pts = sent_samples;
    sent_samples += frame.pointer()->nb_samples;
    encode(frame.pointer(), currentStreamIdx_);
    return 0;
}

int
MediaEncoder::encode(AVFrame* frame, int streamIdx)
{
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
            RING_ERR() << "Failed to encode frame: " << libav_utils::getError(ret);
            return ret;
        }

        if (pkt.size) {
            pkt.stream_index = streamIdx;
            if (pkt.pts != AV_NOPTS_VALUE)
                pkt.pts = av_rescale_q(pkt.pts, encoderCtx->time_base,
                                       outputCtx_->streams[streamIdx]->time_base);
            if (pkt.dts != AV_NOPTS_VALUE)
                pkt.dts = av_rescale_q(pkt.dts, encoderCtx->time_base,
                                       outputCtx_->streams[streamIdx]->time_base);

            // write the compressed frame
            ret = av_write_frame(outputCtx_, &pkt);
            if (ret < 0) {
                RING_ERR() << "av_write_frame failed: " << libav_utils::getError(ret);
            } else
                break;
        }
    }

    av_packet_unref(&pkt);
    return 0;
}

int
MediaEncoder::flush()
{
    int ret = 0;
    for (size_t i = 0; i < outputCtx_->nb_streams; ++i) {
        if (encode(nullptr, i) < 0) {
            RING_ERR() << "Could not flush stream #" << i;
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
    RING_DBG("Sending SDP:\n%s", result.c_str());
#endif
    return result;
}

AVCodecContext* MediaEncoder::prepareEncoderContext(AVCodec* outputCodec, bool is_video)
{
    AVCodecContext* encoderCtx = avcodec_alloc_context3(outputCodec);

    auto encoderName = encoderCtx->av_class->item_name ?
        encoderCtx->av_class->item_name(encoderCtx) : nullptr;
    if (encoderName == nullptr)
        encoderName = "encoder?";

    encoderCtx->thread_count = std::min(std::thread::hardware_concurrency(), is_video ? 16u : 4u);
    RING_DBG("[%s] Using %d threads", encoderName, encoderCtx->thread_count);

    if (is_video) {
        // resolution must be a multiple of two
        if (device_.width && device_.height) {
            encoderCtx->width = device_.width;
            encoderCtx->height = device_.height;
        } else {
            encoderCtx->width = std::atoi(libav_utils::getDictValue(options_, "width"));
            encoderCtx->height = std::atoi(libav_utils::getDictValue(options_, "height"));
        }

        // satisfy ffmpeg: denominator must be 16bit or less value
        // time base = 1/FPS
        if (device_.framerate) {
            av_reduce(&encoderCtx->framerate.num, &encoderCtx->framerate.den,
                      device_.framerate.numerator(), device_.framerate.denominator(),
                      (1U << 16) - 1);
            encoderCtx->time_base = av_inv_q(encoderCtx->framerate);
        } else {
            // get from options_, else default to 30 fps
            auto v = libav_utils::getDictValue(options_, "framerate");
            AVRational framerate = AVRational{30, 1};
            if (v)
                av_parse_ratio_quiet(&framerate, v, 120);
            if (framerate.den == 0)
                framerate.den = 1;
            av_reduce(&encoderCtx->framerate.num, &encoderCtx->framerate.den,
                      framerate.num, framerate.den,
                      (1U << 16) - 1);
            encoderCtx->time_base = av_inv_q(encoderCtx->framerate);
        }

        // emit one intra frame every gop_size frames
        encoderCtx->max_b_frames = 0;
        encoderCtx->pix_fmt = AV_PIX_FMT_YUV420P; // TODO: option me !

        // Fri Jul 22 11:37:59 EDT 2011:tmatth:XXX: DON'T set this, we want our
        // pps and sps to be sent in-band for RTP
        // This is to place global headers in extradata instead of every
        // keyframe.
        // encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    } else {
        encoderCtx->sample_fmt = AV_SAMPLE_FMT_S16;
        auto v = libav_utils::getDictValue(options_, "sample_rate");
        if (v) {
            encoderCtx->sample_rate = atoi(v);
            encoderCtx->time_base = AVRational{1, encoderCtx->sample_rate};
        } else {
            RING_WARN("[%s] No sample rate set", encoderName);
            encoderCtx->sample_rate = 8000;
        }

        v = libav_utils::getDictValue(options_, "channels");
        if (v) {
            auto c = std::atoi(v);
            if (c > 2 or c < 1) {
                RING_WARN("[%s] Clamping invalid channel value %d", encoderName, c);
                c = 1;
            }
            encoderCtx->channels = c;
        } else {
            RING_WARN("[%s] Channels not set", encoderName);
            encoderCtx->channels = 1;
        }

        encoderCtx->channel_layout = encoderCtx->channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;

        v = libav_utils::getDictValue(options_, "frame_size");
        if (v) {
            encoderCtx->frame_size = atoi(v);
            RING_DBG("[%s] Frame size %d", encoderName, encoderCtx->frame_size);
        } else {
            RING_WARN("[%s] Frame size not set", encoderName);
        }
    }

    return encoderCtx;
}

void MediaEncoder::forcePresetX264(AVCodecContext* encoderCtx)
{
    const char *speedPreset = "ultrafast";
    if (av_opt_set(encoderCtx->priv_data, "preset", speedPreset, 0))
        RING_WARN("Failed to set x264 preset '%s'", speedPreset);
    const char *tune = "zerolatency";
    if (av_opt_set(encoderCtx->priv_data, "tune", tune, 0))
        RING_WARN("Failed to set x264 tune '%s'", tune);
}

void MediaEncoder::extractProfileLevelID(const std::string &parameters,
                                         AVCodecContext *ctx)
{
    // From RFC3984:
    // If no profile-level-id is present, the Baseline Profile without
    // additional constraints at Level 1 MUST be implied.
    ctx->profile = FF_PROFILE_H264_BASELINE;
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
    RING_DBG("Using profile %x and level %d", ctx->profile, ctx->level);
}

void
MediaEncoder::setMuted(bool isMuted)
{
    is_muted = isMuted;
}

bool
MediaEncoder::useCodec(const ring::AccountCodecInfo* codec) const noexcept
{
    return codec_.get() == codec;
}

unsigned
MediaEncoder::getStreamCount() const
{
    if (outputCtx_)
        return outputCtx_->nb_streams;
    else
        return 0;
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
    return MediaStream(name, enc);
}

} // namespace ring
