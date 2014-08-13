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

#include "libav_deps.h"
#include "video_encoder.h"
#include "logger.h"

#include <iostream>
#include <sstream>


namespace sfl_video {

using std::string;

VideoEncoder::VideoEncoder() :
    outputCtx_(avformat_alloc_context()),
    scaler_(),
    scaledFrame_()
{}

VideoEncoder::~VideoEncoder()
{
    if (outputCtx_ and outputCtx_->priv_data)
        av_write_trailer(outputCtx_);

    if (encoderCtx_)
        avcodec_close(encoderCtx_);

    av_free(scaledFrameBuffer_);

#if (LIBAVCODEC_VERSION_MAJOR < 54)
    av_free(encoderBuffer_);
#endif
}

static const char *
extract(const std::map<std::string, std::string>& map, const std::string& key)
{
    auto iter = map.find(key);

    if (iter == map.end())
        return NULL;

    return iter->second.c_str();
}

void VideoEncoder::setOptions(const std::map<std::string, std::string>& options)
{
    const char *value;

    value = extract(options, "width");
    if (!value)
        throw VideoEncoderException("width option not set");
    av_dict_set(&options_, "width", value, 0);

    value = extract(options, "height");
    if (!value)
        throw VideoEncoderException("height option not set");
    av_dict_set(&options_, "height", value, 0);

    value = extract(options, "bitrate") ? : "";
    av_dict_set(&options_, "bitrate", value, 0);

    value = extract(options, "framerate");
    if (value)
        av_dict_set(&options_, "framerate", value, 0);

    value = extract(options, "parameters");
    if (value)
        av_dict_set(&options_, "parameters", value, 0);

    value = extract(options, "payload_type");
    if (value)
        av_dict_set(&options_, "payload_type", value, 0);
}

void
VideoEncoder::openOutput(const char *enc_name, const char *short_name,
                             const char *filename, const char *mime_type)
{
    AVOutputFormat *oformat = av_guess_format(short_name, filename, mime_type);

    if (!oformat) {
        ERROR("Unable to find a suitable output format for %s", filename);
        throw VideoEncoderException("No output format");
    }

    outputCtx_->oformat = oformat;
    strncpy(outputCtx_->filename, filename, sizeof(outputCtx_->filename));
    // guarantee that buffer is NULL terminated
    outputCtx_->filename[sizeof(outputCtx_->filename) - 1] = '\0';

    /* find the video encoder */
    outputEncoder_ = avcodec_find_encoder_by_name(enc_name);
    if (!outputEncoder_) {
        ERROR("Encoder \"%s\" not found!", enc_name);
        throw VideoEncoderException("No output encoder");
    }

    prepareEncoderContext();

    /* let x264 preset override our encoder settings */
    if (!strcmp(enc_name, "libx264")) {
        AVDictionaryEntry *entry = av_dict_get(options_, "parameters",
                                               NULL, 0);
        // FIXME: this should be parsed from the fmtp:profile-level-id
        // attribute of our peer, it will determine what profile and
        // level we are sending (i.e. that they can accept).
        extractProfileLevelID(entry?entry->value:"", encoderCtx_);
        forcePresetX264();
    } else if (!strcmp(enc_name, "libvpx")) {
        av_opt_set(encoderCtx_->priv_data, "quality", "realtime", 0);
    }

    int ret;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    ret = avcodec_open(encoderCtx_, outputEncoder_);
#else
    ret = avcodec_open2(encoderCtx_, outputEncoder_, NULL);
#endif
    if (ret)
        throw VideoEncoderException("Could not open encoder");

    // add video stream to outputformat context
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    stream_ = av_new_stream(outputCtx_, 0);
#else
    stream_ = avformat_new_stream(outputCtx_, 0);
#endif
    if (!stream_)
        throw VideoEncoderException("Could not allocate stream");

    stream_->codec = encoderCtx_;

    // allocate buffers for both scaled (pre-encoder) and encoded frames
    const int width = encoderCtx_->width;
    const int height = encoderCtx_->height;
    const int format = libav_utils::sfl_pixel_format((int)encoderCtx_->pix_fmt);
    scaledFrameBufferSize_ = scaledFrame_.getSize(width, height, format);
    if (scaledFrameBufferSize_ <= FF_MIN_BUFFER_SIZE)
        throw VideoEncoderException("buffer too small");

#if (LIBAVCODEC_VERSION_MAJOR < 54)
    encoderBufferSize_ = scaledFrameBufferSize_; // seems to be ok
    encoderBuffer_ = (uint8_t*) av_malloc(encoderBufferSize_);
    if (!encoderBuffer_)
        throw VideoEncoderException("Could not allocate encoder buffer");
#endif

    scaledFrameBuffer_ = (uint8_t*) av_malloc(scaledFrameBufferSize_);
    if (!scaledFrameBuffer_)
        throw VideoEncoderException("Could not allocate scaled frame buffer");

    scaledFrame_.setDestination(scaledFrameBuffer_, width, height, format);
}

void VideoEncoder::setInterruptCallback(int (*cb)(void*), void *opaque)
{
    if (cb) {
        outputCtx_->interrupt_callback.callback = cb;
        outputCtx_->interrupt_callback.opaque = opaque;
    } else {
        outputCtx_->interrupt_callback.callback = 0;
    }
}

void VideoEncoder::setIOContext(const std::unique_ptr<VideoIOHandle> &ioctx)
{
    outputCtx_->pb = ioctx->getContext();
    outputCtx_->packet_size = outputCtx_->pb->buffer_size;
}

void
VideoEncoder::startIO()
{
    if (avformat_write_header(outputCtx_, options_ ? &options_ : NULL)) {
        ERROR("Could not write header for output file... check codec parameters");
        throw VideoEncoderException("Failed to write output file header");
    }

    av_dump_format(outputCtx_, 0, outputCtx_->filename, 1);
}

static void
print_averror(const char *funcname, int err)
{
    char errbuf[64];
    av_strerror(err, errbuf, sizeof(errbuf));
    ERROR("%s failed: %s", funcname, errbuf);
}

int VideoEncoder::encode(VideoFrame &input, bool is_keyframe, int64_t frame_number)
{
    /* Prepare a frame suitable to our encoder frame format,
     * keeping also the input aspect ratio.
     */
    scaledFrame_.clear(); // to fill blank space left by the "keep aspect"
    scaler_.scale_with_aspect(input, scaledFrame_);

    AVFrame *frame = scaledFrame_.get();
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
            pkt.pts = av_rescale_q(pkt.pts, encoderCtx_->time_base, stream_->time_base);
        if (pkt.dts != AV_NOPTS_VALUE)
            pkt.dts = av_rescale_q(pkt.dts, encoderCtx_->time_base, stream_->time_base);

        pkt.stream_index = stream_->index;

        // write the compressed frame
        ret = av_write_frame(outputCtx_, &pkt);
        if (ret < 0)
            print_averror("av_write_frame", ret);
    }

#else

    int ret = avcodec_encode_video(encoderCtx_, encoderBuffer_,
                                   encoderBufferSize_, frame);
    if (ret < 0) {
        print_averror("avcodec_encode_video", ret);
        av_free_packet(&pkt);
        return ret;
    }

    pkt.data = encoderBuffer_;
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

int VideoEncoder::flush()
{
    AVPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    av_init_packet(&pkt);

    int ret;
#if (LIBAVCODEC_VERSION_MAJOR >= 54)

    int got_packet;

    ret = avcodec_encode_video2(encoderCtx_, &pkt, NULL, &got_packet);
    if (ret != 0) {
        ERROR("avcodec_encode_video failed");
        av_free_packet(&pkt);
        return -1;
    }

    if (pkt.size and got_packet) {
        // write the compressed frame
        ret = av_write_frame(outputCtx_, &pkt);
        if (ret < 0)
            ERROR("write_frame failed");
    }
#else
    ret = avcodec_encode_video(encoderCtx_, encoderBuffer_,
                               encoderBufferSize_, NULL);
    if (ret < 0) {
        ERROR("avcodec_encode_video failed");
        av_free_packet(&pkt);
        return ret;
    }

    pkt.data = encoderBuffer_;
    pkt.size = ret;

    // write the compressed frame
    ret = av_write_frame(outputCtx_, &pkt);
    if (ret < 0)
        ERROR("write_frame failed");
#endif
    av_free_packet(&pkt);

    return ret;
}

void VideoEncoder::print_sdp(std::string &sdp_)
{
    /* theora sdp can be huge */
    const size_t sdp_size = outputCtx_->streams[0]->codec->extradata_size \
        + 2048;
    std::string sdp(sdp_size, 0);
    av_sdp_create(&outputCtx_, 1, &(*sdp.begin()), sdp_size);
    std::istringstream iss(sdp);
    string line;
    sdp_ = "";
    while (std::getline(iss, line)) {
        /* strip windows line ending */
        line = line.substr(0, line.length() - 1);
        sdp_ += line + "\n";
    }
    DEBUG("Sending SDP: \n%s", sdp_.c_str());
}

void VideoEncoder::prepareEncoderContext()
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 12, 0)
    encoderCtx_ = avcodec_alloc_context();
    avcodec_get_context_defaults(encoderCtx_);
    (void) outputEncoder_;
#else
    encoderCtx_ = avcodec_alloc_context3(outputEncoder_);
#endif

    // set some encoder settings here
    encoderCtx_->bit_rate = 1000 * atoi(av_dict_get(options_, "bitrate",
                                                    NULL, 0)->value);
    DEBUG("Using bitrate %d", encoderCtx_->bit_rate);

    // resolution must be a multiple of two
    char *width = av_dict_get(options_, "width", NULL, 0)->value;
    dstWidth_ = encoderCtx_->width = width ? atoi(width) : 0;
    char *height = av_dict_get(options_, "height", NULL, 0)->value;
    dstHeight_ = encoderCtx_->height = height ? atoi(height) : 0;

    const char *framerate = av_dict_get(options_, "framerate",
                                        NULL, 0)->value;
    const int DEFAULT_FPS = 30;
    const int fps = framerate ? atoi(framerate) : DEFAULT_FPS;
    encoderCtx_->time_base = (AVRational) {1, fps};
    // emit one intra frame every gop_size frames
    encoderCtx_->max_b_frames = 0;
    encoderCtx_->pix_fmt = PIXEL_FORMAT(YUV420P); // TODO: option me !

    // Fri Jul 22 11:37:59 EDT 2011:tmatth:XXX: DON'T set this, we want our
    // pps and sps to be sent in-band for RTP
    // This is to place global headers in extradata instead of every
    // keyframe.
    // encoderCtx_->flags |= CODEC_FLAG_GLOBAL_HEADER;
}

void VideoEncoder::forcePresetX264()
{
    const char *speedPreset = "ultrafast";
    if (av_opt_set(encoderCtx_->priv_data, "preset", speedPreset, 0))
        WARN("Failed to set x264 preset '%s'", speedPreset);
    const char *tune = "zerolatency";
    if (av_opt_set(encoderCtx_->priv_data, "tune", tune, 0))
        WARN("Failed to set x264 tune '%s'", tune);
}

void VideoEncoder::extractProfileLevelID(const std::string &parameters,
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
    DEBUG("Using profile %x and level %d", ctx->profile, ctx->level);
}

}
