/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Eloi Bail <Eloi.Bail@savoirfairelinux.com>
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

        if (encoderCtx_)
            avcodec_close(encoderCtx_);

        avformat_free_context(outputCtx_);
    }

    av_dict_free(&options_);
}

void MediaEncoder::setDeviceOptions(const DeviceParams& args)
{
    device_ = args;
    if (device_.width)
        av_dict_set(&options_, "width", ring::to_string(device_.width).c_str(), 0);
    if (device_.height)
        av_dict_set(&options_, "height", ring::to_string(device_.height).c_str(), 0);
    if (not device_.framerate)
        device_.framerate = 30;
    av_dict_set(&options_, "framerate", ring::to_string(device_.framerate.real()).c_str(), 0);
}

void MediaEncoder::setOptions(const MediaDescription& args)
{
    codec_ = args.codec;

    av_dict_set(&options_, "payload_type", ring::to_string(args.payload_type).c_str(), 0);
    av_dict_set(&options_, "max_rate", ring::to_string(args.codec->bitrate).c_str(), 0);
    av_dict_set(&options_, "crf", ring::to_string(args.codec->quality).c_str(), 0);

    if (args.codec->systemCodecInfo.mediaType == MEDIA_AUDIO) {
        auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(args.codec);
        if (accountAudioCodec->audioformat.sample_rate)
            av_dict_set(&options_, "sample_rate",
                        ring::to_string(accountAudioCodec->audioformat.sample_rate).c_str(), 0);

        if (accountAudioCodec->audioformat.nb_channels)
            av_dict_set(&options_, "channels",
                        ring::to_string(accountAudioCodec->audioformat.nb_channels).c_str(), 0);

        if (accountAudioCodec->audioformat.sample_rate && accountAudioCodec->audioformat.nb_channels)
            av_dict_set(&options_, "frame_size",
                        ring::to_string(static_cast<unsigned>(0.02 * accountAudioCodec->audioformat.sample_rate)).c_str(), 0);
    }

    if (not args.parameters.empty())
        av_dict_set(&options_, "parameters", args.parameters.c_str(), 0);
}

void
MediaEncoder::setInitSeqVal(uint16_t seqVal)
{
    //only set not default value (!=0)
    if (seqVal != 0)
        av_dict_set(&options_, "seq", ring::to_string(seqVal).c_str(), 0);
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
    return encoderCtx_->codec->name;
}

void
MediaEncoder::openOutput(const char *filename,
                         const ring::MediaDescription& args)
{
    setOptions(args);
    AVOutputFormat *oformat = av_guess_format("rtp", filename, nullptr);

    if (!oformat) {
        RING_ERR("Unable to find a suitable output format for %s", filename);
        throw MediaEncoderException("No output format");
    }

    outputCtx_->oformat = oformat;
    strncpy(outputCtx_->filename, filename, sizeof(outputCtx_->filename));
    // guarantee that buffer is NULL terminated
    outputCtx_->filename[sizeof(outputCtx_->filename) - 1] = '\0';

    /* find the video encoder */
    if (args.codec->systemCodecInfo.avcodecId == AV_CODEC_ID_H263)
        // For H263 encoding, we force the use of AV_CODEC_ID_H263P (H263-1998)
        // H263-1998 can manage all frame sizes while H263 don't
        // AV_CODEC_ID_H263 decoder will be used for decoding
        outputEncoder_ = avcodec_find_encoder(AV_CODEC_ID_H263P);
    else
        outputEncoder_ = avcodec_find_encoder((AVCodecID)args.codec->systemCodecInfo.avcodecId);
    if (!outputEncoder_) {
        RING_ERR("Encoder \"%s\" not found!", args.codec->systemCodecInfo.name.c_str());
        throw MediaEncoderException("No output encoder");
    }

    prepareEncoderContext(args.codec->systemCodecInfo.mediaType == MEDIA_VIDEO);
    auto maxBitrate = 1000 * atoi(av_dict_get(options_, "max_rate", NULL, 0)->value);
    auto crf = atoi(av_dict_get(options_, "crf", NULL, 0)->value);


    /* let x264 preset override our encoder settings */
    if (args.codec->systemCodecInfo.avcodecId == AV_CODEC_ID_H264) {
        extractProfileLevelID(args.parameters, encoderCtx_);
        forcePresetX264();
        // For H264 :
        // 1- if quality is set use it
        // 2- otherwise set rc_max_rate and rc_buffer_size
        if (crf != SystemCodecInfo::DEFAULT_NO_QUALITY) {
            av_opt_set(encoderCtx_->priv_data, "crf", av_dict_get(options_, "crf", NULL, 0)->value, 0);
            RING_DBG("Using quality factor %d", crf);
        } else {
            encoderCtx_->rc_buffer_size = maxBitrate;
            encoderCtx_->rc_max_rate = maxBitrate;
            RING_DBG("Using max bitrate %d", maxBitrate );
        }

    } else if (args.codec->systemCodecInfo.avcodecId == AV_CODEC_ID_VP8) {
        // For VP8 :
        // 1- if quality is set use it
        // bitrate need to be set. The target bitrate becomes the maximum allowed bitrate
        // 2- otherwise set rc_max_rate and rc_buffer_size
        // Using information given on this page:
        // http://www.webmproject.org/docs/encoder-parameters/
        av_opt_set(encoderCtx_->priv_data, "quality", "realtime", 0);
        av_opt_set_int(encoderCtx_->priv_data, "error-resilient", 1, 0);
        av_opt_set_int(encoderCtx_->priv_data, "cpu-used", 3, 0);
        av_opt_set_int(encoderCtx_->priv_data, "lag-in-frames", 0, 0);
        encoderCtx_->slices = 2; // VP8E_SET_TOKEN_PARTITIONS
        encoderCtx_->qmin = 4;
        encoderCtx_->qmax = 56;
        encoderCtx_->gop_size = 999999;

        encoderCtx_->rc_buffer_size = maxBitrate;
        encoderCtx_->bit_rate = maxBitrate;
        if (crf != SystemCodecInfo::DEFAULT_NO_QUALITY) {
            av_opt_set(encoderCtx_->priv_data, "crf", av_dict_get(options_, "crf", NULL, 0)->value, 0);
            RING_DBG("Using quality factor %d", crf);
        } else {
            RING_DBG("Using Max bitrate %d", maxBitrate);
        }
    } else if (args.codec->systemCodecInfo.avcodecId == AV_CODEC_ID_MPEG4) {
        // For MPEG4 :
        // No CRF avaiable.
        // Use CBR (set bitrate)
        encoderCtx_->rc_buffer_size = maxBitrate;
        encoderCtx_->bit_rate = encoderCtx_->rc_min_rate = encoderCtx_->rc_max_rate =  maxBitrate;
        RING_DBG("Using Max bitrate %d", maxBitrate);
    } else if (args.codec->systemCodecInfo.avcodecId == AV_CODEC_ID_H263) {
        encoderCtx_->bit_rate = encoderCtx_->rc_max_rate =  maxBitrate;
        encoderCtx_->rc_buffer_size = maxBitrate;
        RING_DBG("Using Max bitrate %d", maxBitrate);
    }

    int ret;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    ret = avcodec_open(encoderCtx_, outputEncoder_);
#else
    ret = avcodec_open2(encoderCtx_, outputEncoder_, NULL);
#endif
    if (ret)
        throw MediaEncoderException("Could not open encoder");

    // add video stream to outputformat context
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    stream_ = av_new_stream(outputCtx_, 0);
#else
    stream_ = avformat_new_stream(outputCtx_, 0);
#endif
    if (!stream_)
        throw MediaEncoderException("Could not allocate stream");

    stream_->codec = encoderCtx_;
#ifdef RING_VIDEO
    if (args.codec->systemCodecInfo.mediaType == MEDIA_VIDEO) {
        // allocate buffers for both scaled (pre-encoder) and encoded frames
        const int width = encoderCtx_->width;
        const int height = encoderCtx_->height;
        const int format = libav_utils::ring_pixel_format((int)encoderCtx_->pix_fmt);
        scaledFrameBufferSize_ = videoFrameSize(format, width, height);
        if (scaledFrameBufferSize_ <= FF_MIN_BUFFER_SIZE)
            throw MediaEncoderException("buffer too small");

#if (LIBAVCODEC_VERSION_MAJOR < 54)
        encoderBufferSize_ = scaledFrameBufferSize_; // seems to be ok
        encoderBuffer_.reserve(encoderBufferSize_);
#endif

        scaledFrameBuffer_.reserve(scaledFrameBufferSize_);
        scaledFrame_.setFromMemory(scaledFrameBuffer_.data(), format, width, height);
    }
#endif // RING_VIDEO
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
    outputCtx_->pb = ioctx->getContext();
    outputCtx_->packet_size = outputCtx_->pb->buffer_size;
}

void
MediaEncoder::startIO()
{
    if (avformat_write_header(outputCtx_, options_ ? &options_ : NULL)) {
        RING_ERR("Could not write header for output file... check codec parameters");
        throw MediaEncoderException("Failed to write output file header");
    }

    av_dump_format(outputCtx_, 0, outputCtx_->filename, 1);
}

static void
print_averror(const char *funcname, int err)
{
    char errbuf[64];
    av_strerror(err, errbuf, sizeof(errbuf));
    RING_ERR("%s failed: %s", funcname, errbuf);
}

#ifdef RING_VIDEO
int
MediaEncoder::encode(VideoFrame& input, bool is_keyframe,
                     int64_t frame_number)
{
    /* Prepare a frame suitable to our encoder frame format,
     * keeping also the input aspect ratio.
     */
    yuv422_clear_to_black(scaledFrame_); // to fill blank space left by the "keep aspect"

    scaler_.scale_with_aspect(input, scaledFrame_);

    auto frame = scaledFrame_.pointer();
    frame->pts = frame_number;

    if (is_keyframe) {
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(53, 20, 0)
        frame->pict_type = AV_PICTURE_TYPE_I;
#else
        frame->pict_type = FF_I_TYPE;
#endif
    } else {
        /* FIXME: Should be AV_PICTURE_TYPE_NONE for newer libavutil */
        frame->pict_type = (AVPictureType) 0;
    }

    AVPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    av_init_packet(&pkt);

#if LIBAVCODEC_VERSION_MAJOR >= 54

    int got_packet;
    int ret = avcodec_encode_video2(encoderCtx_, &pkt, frame, &got_packet);
    if (ret < 0) {
        print_averror("avcodec_encode_video2", ret);
        av_free_packet(&pkt);
        return ret;
    }

    if (pkt.size and got_packet) {
        if (pkt.pts != AV_NOPTS_VALUE)
            pkt.pts = av_rescale_q(pkt.pts, encoderCtx_->time_base,
                                   stream_->time_base);
        if (pkt.dts != AV_NOPTS_VALUE)
            pkt.dts = av_rescale_q(pkt.dts, encoderCtx_->time_base,
                                   stream_->time_base);

        pkt.stream_index = stream_->index;

        // write the compressed frame
        ret = av_write_frame(outputCtx_, &pkt);
        if (ret < 0)
            print_averror("av_write_frame", ret);
    }

#else

    int ret = avcodec_encode_video(encoderCtx_, encoderBuffer_.data(),
                                   encoderBufferSize_, frame);
    if (ret < 0) {
        print_averror("avcodec_encode_video", ret);
        av_free_packet(&pkt);
        return ret;
    }

    pkt.data = encoderBuffer_.data();
    pkt.size = ret;

    // rescale pts from encoded video framerate to rtp clock rate
    if (encoderCtx_->coded_frame->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
        pkt.pts = av_rescale_q(encoderCtx_->coded_frame->pts,
                encoderCtx_->time_base, stream_->time_base);
     } else {
         pkt.pts = 0;
     }

     // is it a key frame?
     if (encoderCtx_->coded_frame->key_frame)
         pkt.flags |= AV_PKT_FLAG_KEY;
     pkt.stream_index = stream_->index;

    // write the compressed frame
    ret = av_write_frame(outputCtx_, &pkt);
    if (ret < 0)
        print_averror("av_write_frame", ret);

#endif  // LIBAVCODEC_VERSION_MAJOR >= 54

    av_free_packet(&pkt);

    return ret;
}
#endif // RING_VIDEO

int MediaEncoder::encode_audio(const AudioBuffer &buffer)
{
    const int needed_bytes = av_samples_get_buffer_size(NULL, buffer.channels(),
                                                        buffer.frames(),
                                                        AV_SAMPLE_FMT_S16, 0);
    if (needed_bytes < 0) {
        RING_ERR("Couldn't calculate buffer size");
        return -1;
    }

    std::vector<AudioSample> samples (needed_bytes / sizeof(AudioSample));
    AudioSample* sample_data = samples.data();

    AudioSample *offset_ptr = sample_data;
    int nb_frames = buffer.frames();

    if (not is_muted) {
        //only fill buffer with samples if not muted
        buffer.interleave(sample_data);
    } else {
        //otherwise filll buffer with zero
        buffer.fillWithZero(sample_data);
    }
    const auto layout = buffer.channels() == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
    const auto sample_rate = buffer.getSampleRate();

    while (nb_frames > 0) {
        AVFrame *frame = av_frame_alloc();
        if (!frame)
            return -1;

        if (encoderCtx_->frame_size)
            frame->nb_samples = std::min<int>(nb_frames,
                                              encoderCtx_->frame_size);
        else
            frame->nb_samples = nb_frames;

        frame->format = AV_SAMPLE_FMT_S16;
        frame->channel_layout = layout;
        frame->sample_rate = sample_rate;

        frame->pts = sent_samples;
        sent_samples += frame->nb_samples;

        const auto buffer_size = \
            av_samples_get_buffer_size(NULL, buffer.channels(),
                                       frame->nb_samples, AV_SAMPLE_FMT_S16, 0);

        int err = avcodec_fill_audio_frame(frame, buffer.channels(),
                                           AV_SAMPLE_FMT_S16,
                                           reinterpret_cast<const uint8_t *>(offset_ptr),
                                           buffer_size, 0);
        if (err < 0) {
            char errbuf[128];
            av_strerror(err, errbuf, sizeof(errbuf));
            RING_ERR("Couldn't fill audio frame: %s: %d %d", errbuf,
                     frame->nb_samples, buffer_size);
            av_frame_free(&frame);
            return -1;
        }

        nb_frames -= frame->nb_samples;
        offset_ptr += frame->nb_samples * buffer.channels();

        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = NULL; // packet data will be allocated by the encoder
        pkt.size = 0;

        int got_packet;
        int ret = avcodec_encode_audio2(encoderCtx_, &pkt, frame, &got_packet);
        if (ret < 0) {
            print_averror("avcodec_encode_audio2", ret);
            av_free_packet(&pkt);
            av_frame_free(&frame);
            return ret;
        }

        if (pkt.size and got_packet) {
            if (pkt.pts != AV_NOPTS_VALUE)
                pkt.pts = av_rescale_q(pkt.pts, encoderCtx_->time_base,
                                       stream_->time_base);
            if (pkt.dts != AV_NOPTS_VALUE)
                pkt.dts = av_rescale_q(pkt.dts, encoderCtx_->time_base,
                                       stream_->time_base);

            pkt.stream_index = stream_->index;

            // write the compressed frame
            ret = av_write_frame(outputCtx_, &pkt);
            if (ret < 0)
                print_averror("av_write_frame", ret);
        }

        av_free_packet(&pkt);
        av_frame_free(&frame);
    }

    return 0;
}

int MediaEncoder::flush()
{
    AVPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    av_init_packet(&pkt);

    int ret;
#if (LIBAVCODEC_VERSION_MAJOR >= 54)

    int got_packet;

    ret = avcodec_encode_video2(encoderCtx_, &pkt, NULL, &got_packet);
    if (ret != 0) {
        RING_ERR("avcodec_encode_video failed");
        av_free_packet(&pkt);
        return -1;
    }

    if (pkt.size and got_packet) {
        // write the compressed frame
        ret = av_write_frame(outputCtx_, &pkt);
        if (ret < 0)
            RING_ERR("write_frame failed");
    }
#else
    ret = avcodec_encode_video(encoderCtx_, encoderBuffer_.data(),
                               encoderBufferSize_, NULL);
    if (ret < 0) {
        RING_ERR("avcodec_encode_video failed");
        av_free_packet(&pkt);
        return ret;
    }

    pkt.data = encoderBuffer_.data();
    pkt.size = ret;

    // write the compressed frame
    ret = av_write_frame(outputCtx_, &pkt);
    if (ret < 0)
        RING_ERR("write_frame failed");
#endif
    av_free_packet(&pkt);

    return ret;
}

std::string
MediaEncoder::print_sdp()
{
    /* theora sdp can be huge */
    const auto sdp_size = outputCtx_->streams[0]->codec->extradata_size + 2048;
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

void MediaEncoder::prepareEncoderContext(bool is_video)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 12, 0)
    encoderCtx_ = avcodec_alloc_context();
    avcodec_get_context_defaults(encoderCtx_);
    (void) outputEncoder_;
#else
    encoderCtx_ = avcodec_alloc_context3(outputEncoder_);
#endif

    auto encoderName = encoderCtx_->av_class->item_name ?
        encoderCtx_->av_class->item_name(encoderCtx_) : nullptr;
    if (encoderName == nullptr)
        encoderName = "encoder?";


    encoderCtx_->thread_count = std::thread::hardware_concurrency();
    RING_DBG("[%s] Using %d threads", encoderName, encoderCtx_->thread_count);


    if (is_video) {
        // resolution must be a multiple of two
        encoderCtx_->width = device_.width;
        encoderCtx_->height = device_.height;

        // time base = 1/FPS
        encoderCtx_->time_base = AVRational {
            (int) device_.framerate.denominator(),
            (int) device_.framerate.numerator()
        };

        // emit one intra frame every gop_size frames
        encoderCtx_->max_b_frames = 0;
        encoderCtx_->pix_fmt = PIXEL_FORMAT(YUV420P); // TODO: option me !

        // Fri Jul 22 11:37:59 EDT 2011:tmatth:XXX: DON'T set this, we want our
        // pps and sps to be sent in-band for RTP
        // This is to place global headers in extradata instead of every
        // keyframe.
        // encoderCtx_->flags |= CODEC_FLAG_GLOBAL_HEADER;

    } else {
        encoderCtx_->sample_fmt = AV_SAMPLE_FMT_S16;
        auto v = av_dict_get(options_, "sample_rate", NULL, 0);
        if (v) {
            encoderCtx_->sample_rate = atoi(v->value);
            encoderCtx_->time_base = (AVRational) {1, encoderCtx_->sample_rate};
        } else {
            RING_WARN("[%s] No sample rate set", encoderName);
            encoderCtx_->sample_rate = 8000;
        }

        v = av_dict_get(options_, "channels", NULL, 0);
        if (v) {
            auto c = std::atoi(v->value);
            if (c > 2 or c < 1) {
                RING_WARN("[%s] Clamping invalid channel value %d", encoderName, c);
                c = 1;
            }
            encoderCtx_->channels = c;
        } else {
            RING_WARN("[%s] Channels not set", encoderName);
            encoderCtx_->channels = 1;
        }

        encoderCtx_->channel_layout = encoderCtx_->channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;

        v = av_dict_get(options_, "frame_size", NULL, 0);
        if (v) {
            encoderCtx_->frame_size = atoi(v->value);
            RING_DBG("[%s] Frame size %d", encoderName, encoderCtx_->frame_size);
        } else {
            RING_WARN("[%s] Frame size not set", encoderName);
        }
    }
}

void MediaEncoder::forcePresetX264()
{
    const char *speedPreset = "ultrafast";
    if (av_opt_set(encoderCtx_->priv_data, "preset", speedPreset, 0))
        RING_WARN("Failed to set x264 preset '%s'", speedPreset);
    const char *tune = "zerolatency";
    if (av_opt_set(encoderCtx_->priv_data, "tune", tune, 0))
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

} // namespace ring
