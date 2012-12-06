/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "video_send_thread.h"
#include "packet_handle.h"
#include "check.h"

// libav includes
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/mathematics.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

#include <map>
#include "manager.h"

namespace sfl_video {

using std::string;

void VideoSendThread::print_sdp()
{
    /* theora sdp can be huge */
    const size_t sdp_size = outputCtx_->streams[0]->codec->extradata_size + 2048;
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
    DEBUG("sending\n%s", sdp_.c_str());
}

void VideoSendThread::forcePresetX264()
{
    const char *speedPreset = "ultrafast";
    if (av_opt_set(encoderCtx_->priv_data, "preset", speedPreset, 0))
        WARN("Failed to set x264 preset '%s'", speedPreset);
    const char *tune = "zerolatency";
    if (av_opt_set(encoderCtx_->priv_data, "tune", tune, 0))
        WARN("Failed to set x264 tune '%s'", tune);
}

void VideoSendThread::prepareEncoderContext(AVCodec *encoder)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 12, 0)
    encoderCtx_ = avcodec_alloc_context();
    avcodec_get_context_defaults(encoderCtx_);
    (void) encoder;
#else
    encoderCtx_ = avcodec_alloc_context3(encoder);
#endif

    // set some encoder settings here
    encoderCtx_->bit_rate = 1000 * atoi(args_["bitrate"].c_str());
    DEBUG("Using bitrate %d", encoderCtx_->bit_rate);

    // resolution must be a multiple of two
    if (args_["width"].empty() and inputDecoderCtx_)
        encoderCtx_->width = inputDecoderCtx_->width;
    else
        encoderCtx_->width = atoi(args_["width"].c_str());

    if (args_["height"].empty() and inputDecoderCtx_)
        encoderCtx_->height = inputDecoderCtx_->height;
    else
        encoderCtx_->height = atoi(args_["height"].c_str());

    const int DEFAULT_FPS = 30;
    const int fps = args_["framerate"].empty() ? DEFAULT_FPS : atoi(args_["framerate"].c_str());
    encoderCtx_->time_base = (AVRational) {1, fps};
    // emit one intra frame every gop_size frames
    encoderCtx_->max_b_frames = 0;
    encoderCtx_->pix_fmt = PIX_FMT_YUV420P;

    // Fri Jul 22 11:37:59 EDT 2011:tmatth:XXX: DON'T set this, we want our
    // pps and sps to be sent in-band for RTP
    // This is to place global headers in extradata instead of every keyframe.
    // encoderCtx_->flags |= CODEC_FLAG_GLOBAL_HEADER;
}

namespace {
void
extractProfileLevelID(const std::string &parameters, AVCodecContext *ctx)
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
            ctx->profile |= (profile_iop & 0x40) >> 6 ? FF_PROFILE_H264_CONSTRAINED : 0;
            break;
        case FF_PROFILE_H264_HIGH_10:
        case FF_PROFILE_H264_HIGH_422:
        case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
            // check constraint_set_3_flag
            ctx->profile |= (profile_iop & 0x10) >> 4 ? FF_PROFILE_H264_INTRA : 0;
            break;
    }
    DEBUG("Using profile %x and level %d", ctx->profile, ctx->level);
}
}

void VideoSendThread::setup()
{
    AVInputFormat *file_iformat = 0;
    const char *enc_name = args_["codec"].c_str();
    // it's a v4l device if starting with /dev/video
    static const char * const V4L_PATH = "/dev/video";
    if (args_["input"].find(V4L_PATH) != std::string::npos) {
        DEBUG("Using v4l2 format");
        file_iformat = av_find_input_format("video4linux2");
        EXIT_IF_FAIL(file_iformat, "Could not find format video4linux2");
    }

    AVDictionary *options = NULL;
    if (!args_["framerate"].empty())
        av_dict_set(&options, "framerate", args_["framerate"].c_str(), 0);
    if (!args_["video_size"].empty())
        av_dict_set(&options, "video_size", args_["video_size"].c_str(), 0);
    if (!args_["channel"].empty())
        av_dict_set(&options, "channel", args_["channel"].c_str(), 0);

    // Open video file
    inputCtx_ = avformat_alloc_context();
    inputCtx_->interrupt_callback = interruptCb_;
    int ret = avformat_open_input(&inputCtx_, args_["input"].c_str(),
                                  file_iformat, options ? &options : NULL);
    if (ret < 0) {
        if (options)
            av_dict_free(&options);
        EXIT_IF_FAIL(false, "Could not open input file %s", args_["input"].c_str());
    }
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    ret = av_find_stream_info(inputCtx_);
#else
    ret = avformat_find_stream_info(inputCtx_, options ? &options : NULL);
#endif
    if (options)
        av_dict_free(&options);
    EXIT_IF_FAIL(ret >= 0, "Couldn't find stream info");

    // find the first video stream from the input
    streamIndex_ = -1;
    for (unsigned i = 0; streamIndex_ == -1 && i < inputCtx_->nb_streams; ++i)
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            streamIndex_ = i;

    EXIT_IF_FAIL(streamIndex_ != -1, "Could not find video stream");

    // Get a pointer to the codec context for the video stream
    inputDecoderCtx_ = inputCtx_->streams[streamIndex_]->codec;
    EXIT_IF_FAIL(inputDecoderCtx_, "Could not get input codec context");

    // find the decoder for the video stream
    AVCodec *inputDecoder = avcodec_find_decoder(inputDecoderCtx_->codec_id);
    EXIT_IF_FAIL(inputDecoder, "Could not decode video stream");

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    ret = avcodec_open(inputDecoderCtx_, inputDecoder);
#else
    ret = avcodec_open2(inputDecoderCtx_, inputDecoder, NULL);
#endif
    EXIT_IF_FAIL(ret >= 0, "Could not open codec");

    outputCtx_ = avformat_alloc_context();
    outputCtx_->interrupt_callback = interruptCb_;

    AVOutputFormat *file_oformat = av_guess_format("rtp", args_["destination"].c_str(), NULL);
    EXIT_IF_FAIL(file_oformat, "Unable to find a suitable output format for %s",
          args_["destination"].c_str());

    outputCtx_->oformat = file_oformat;
    strncpy(outputCtx_->filename, args_["destination"].c_str(),
            sizeof outputCtx_->filename);

    /* find the video encoder */
    AVCodec *encoder = avcodec_find_encoder_by_name(enc_name);
    EXIT_IF_FAIL(encoder != 0, "Encoder \"%s\" not found!", enc_name);

    prepareEncoderContext(encoder);

    /* let x264 preset override our encoder settings */
    if (args_["codec"] == "libx264") {
        // FIXME: this should be parsed from the fmtp:profile-level-id
        // attribute of our peer, it will determine what profile and
        // level we are sending (i.e. that they can accept).
        extractProfileLevelID(args_["parameters"], encoderCtx_);
        forcePresetX264();
    }

    scaledPicture_ = avcodec_alloc_frame();

    // open encoder

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    EXIT_IF_FAIL(avcodec_open(encoderCtx_, encoder) >= 0, "Could not open encoder")
#else
    EXIT_IF_FAIL(avcodec_open2(encoderCtx_, encoder, NULL) >= 0, "Could not open "
          "encoder")
#endif

    // add video stream to outputformat context
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    stream_ = av_new_stream(outputCtx_, 0);
#else
    stream_ = avformat_new_stream(outputCtx_, 0);
#endif
    EXIT_IF_FAIL(stream_ != 0, "Could not allocate stream.");
    stream_->codec = encoderCtx_;

    // open the output file, if needed
    if (!(file_oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open2(&outputCtx_->pb, outputCtx_->filename, AVIO_FLAG_WRITE, &interruptCb_, NULL);
        EXIT_IF_FAIL(ret >= 0, "Could not open \"%s\"!", outputCtx_->filename);
    } else
        DEBUG("No need to open \"%s\"", outputCtx_->filename);

    AVDictionary *outOptions = NULL;
    // write the stream header, if any
    if (not args_["payload_type"].empty()) {
        DEBUG("Writing stream header for payload type %s", args_["payload_type"].c_str());
        av_dict_set(&outOptions, "payload_type", args_["payload_type"].c_str(), 0);
    }


    ret = avformat_write_header(outputCtx_, outOptions ? &outOptions : NULL);
    if (outOptions)
        av_dict_free(&outOptions);
    EXIT_IF_FAIL(ret >= 0, "Could not write header for output file...check codec parameters");

    av_dump_format(outputCtx_, 0, outputCtx_->filename, 1);
    print_sdp();

    // allocate video frame
    rawFrame_ = avcodec_alloc_frame();

    // alloc image and output buffer
    outbufSize_ = encoderCtx_->width * encoderCtx_->height;
    outbuf_ = reinterpret_cast<uint8_t*>(av_malloc(outbufSize_));
    // allocate buffer that fits YUV 420
    scaledPictureBuf_ = reinterpret_cast<uint8_t*>(av_malloc((outbufSize_ * 3) / 2));

    scaledPicture_->data[0] = reinterpret_cast<uint8_t*>(scaledPictureBuf_);
    scaledPicture_->data[1] = scaledPicture_->data[0] + outbufSize_;
    scaledPicture_->data[2] = scaledPicture_->data[1] + outbufSize_ / 4;
    scaledPicture_->linesize[0] = encoderCtx_->width;
    scaledPicture_->linesize[1] = encoderCtx_->width / 2;
    scaledPicture_->linesize[2] = encoderCtx_->width / 2;
}

void VideoSendThread::createScalingContext()
{
    // Create scaling context
    imgConvertCtx_ = sws_getCachedContext(imgConvertCtx_,
                                          inputDecoderCtx_->width,
                                          inputDecoderCtx_->height,
                                          inputDecoderCtx_->pix_fmt,
                                          encoderCtx_->width,
                                          encoderCtx_->height,
                                          encoderCtx_->pix_fmt, SWS_BICUBIC,
                                          NULL, NULL, NULL);
    EXIT_IF_FAIL(imgConvertCtx_, "Cannot init the conversion context");
}

// This callback is used by libav internally to break out of blocking calls
int VideoSendThread::interruptCb(void *ctx)
{
    VideoSendThread *context = static_cast<VideoSendThread*>(ctx);
    return not context->threadRunning_;
}

VideoSendThread::VideoSendThread(const std::map<string, string> &args) :
    args_(args), scaledPictureBuf_(0), outbuf_(0),
    inputDecoderCtx_(0), rawFrame_(0), scaledPicture_(0),
    streamIndex_(-1), outbufSize_(0), encoderCtx_(0), stream_(0),
    inputCtx_(0), outputCtx_(0), imgConvertCtx_(0), sdp_(), interruptCb_(),
    threadRunning_(false), forceKeyFrame_(0), thread_(), frameNumber_(0)
{
    interruptCb_.callback = interruptCb;
    interruptCb_.opaque = this;
}

struct VideoTxContextHandle {
    VideoTxContextHandle(VideoSendThread &tx) : tx_(tx) {}

    ~VideoTxContextHandle()
    {
        if (tx_.imgConvertCtx_)
            sws_freeContext(tx_.imgConvertCtx_);

        // write the trailer, if any.  the trailer must be written
        // before you close the CodecContexts open when you wrote the
        // header; otherwise write_trailer may try to use memory that
        // was freed on av_codec_close()
        if (tx_.outputCtx_ and tx_.outputCtx_->priv_data) {
            av_write_trailer(tx_.outputCtx_);
            if (tx_.outputCtx_->pb)
                avio_close(tx_.outputCtx_->pb);
        }

        if (tx_.scaledPictureBuf_)
            av_free(tx_.scaledPictureBuf_);

        if (tx_.outbuf_)
            av_free(tx_.outbuf_);

        // free the scaled frame
        if (tx_.scaledPicture_)
            av_free(tx_.scaledPicture_);

        // free the YUV frame
        if (tx_.rawFrame_)
            av_free(tx_.rawFrame_);

        // close the codecs
        if (tx_.encoderCtx_) {
            avcodec_close(tx_.encoderCtx_);
            av_freep(&tx_.encoderCtx_);
        }

        // doesn't need to be freed, we didn't use avcodec_alloc_context
        if (tx_.inputDecoderCtx_)
            avcodec_close(tx_.inputDecoderCtx_);

        // close the video file
        if (tx_.inputCtx_)
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
            av_close_input_file(tx_.inputCtx_);
#else
        avformat_close_input(&tx_.inputCtx_);
#endif
    }
    VideoSendThread &tx_;
};


void VideoSendThread::start()
{
    threadRunning_ = true;
    pthread_create(&thread_, NULL, &runCallback, this);
}


void *VideoSendThread::runCallback(void *data)
{
    VideoSendThread *context = static_cast<VideoSendThread*>(data);
    context->run();
    return NULL;
}


void VideoSendThread::run()
{
    // We don't want setup() called in the main thread in case it exits or blocks
    VideoTxContextHandle handle(*this);
    setup();
    createScalingContext();

    while (threadRunning_)
        if (captureFrame())
            encodeAndSendVideo();
}


bool VideoSendThread::captureFrame()
{
    AVPacket inpacket;
    int ret = av_read_frame(inputCtx_, &inpacket);

    if (ret == AVERROR(EAGAIN))
        return false;
    else if (ret < 0)
        EXIT_IF_FAIL(false, "Could not read frame");

    // Guarantees that we free the packet allocated by av_read_frame
    PacketHandle inpacket_handle(inpacket);

    // is this a packet from the video stream?
    if (inpacket.stream_index != streamIndex_)
        return false;

    // decode video frame from camera
    int frameFinished = 0;
    avcodec_decode_video2(inputDecoderCtx_, rawFrame_, &frameFinished,
                          &inpacket);
    if (!frameFinished)
        return false;

    createScalingContext();
    sws_scale(imgConvertCtx_, rawFrame_->data, rawFrame_->linesize, 0,
            inputDecoderCtx_->height, scaledPicture_->data,
            scaledPicture_->linesize);

    // Set presentation timestamp on our scaled frame before encoding it
    scaledPicture_->pts = frameNumber_++;

    return true;
}


void VideoSendThread::encodeAndSendVideo()
{
    if (forceKeyFrame_ > 0) {
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(53, 20, 0)
        scaledPicture_->pict_type = AV_PICTURE_TYPE_I;
#else
        scaledPicture_->pict_type = FF_I_TYPE;
#endif
        atomic_decrement(&forceKeyFrame_);
    }

    const int encodedSize = avcodec_encode_video(encoderCtx_, outbuf_,
                                                 outbufSize_, scaledPicture_);

    if (encodedSize <= 0)
        return;

    AVPacket opkt;
    av_init_packet(&opkt);
    PacketHandle opkt_handle(opkt);

    opkt.data = outbuf_;
    opkt.size = encodedSize;

    // rescale pts from encoded video framerate to rtp
    // clock rate
    if (encoderCtx_->coded_frame->pts != static_cast<int64_t>(AV_NOPTS_VALUE))
        opkt.pts = av_rescale_q(encoderCtx_->coded_frame->pts,
                encoderCtx_->time_base,
                stream_->time_base);
    else
        opkt.pts = 0;

    // is it a key frame?
    if (encoderCtx_->coded_frame->key_frame)
        opkt.flags |= AV_PKT_FLAG_KEY;
    opkt.stream_index = stream_->index;

    // write the compressed frame to the output
    EXIT_IF_FAIL(av_interleaved_write_frame(outputCtx_, &opkt) >= 0,
                 "interleaved_write_frame failed");
}

VideoSendThread::~VideoSendThread()
{
    set_false_atomic(&threadRunning_);
    pthread_join(thread_, NULL);
}

void VideoSendThread::forceKeyFrame()
{
    atomic_increment(&forceKeyFrame_);
}

} // end namespace sfl_video
