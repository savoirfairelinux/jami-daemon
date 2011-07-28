/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#define __STDC_CONSTANT_MACROS

#include "video_send_thread.h"

// libav includes
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

#include "manager.h"
#include "libx264-ultrafast.ffpreset.h"

namespace
{
    void print_error(const char *msg, int err)
    {
        char errbuf[128];
        const char *errbuf_ptr = errbuf;

        if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
            errbuf_ptr = strerror(AVUNERROR(err));
        _error("%s:%s", msg, errbuf_ptr);
    }
} // end anonymous namespace

namespace sfl_video {

void VideoSendThread::print_and_save_sdp()
{
    size_t sdp_size = outputCtx_->streams[0]->codec->extradata_size + 2048;
    char *sdp = reinterpret_cast<char*>(malloc(sdp_size)); /* theora sdp can be huge */
    _debug("VideoSendThread:sdp_size: %d", sdp_size);
    av_sdp_create(&outputCtx_, 1, sdp, sdp_size);
    std::istringstream iss(sdp);
    std::string line;
    sdp_ = "";
    while (std::getline(iss, line))
    {
        /* strip windows line ending */
        line = line.substr(0, line.length() - 1);
        sdp_ += line + "\n";
    }
    _debug("%s", sdp_.c_str());
    free(sdp);
    sdpReady_.signal();
}

// NOT called from this (the run() ) thread
void VideoSendThread::waitForSDP()
{
    sdpReady_.wait();
}

void VideoSendThread::forcePresetX264()
{
    av_set_options_string(encoderCtx_, x264_preset_ultrafast, "=", "\n");
}

void VideoSendThread::prepareEncoderContext()
{
    encoderCtx_ = avcodec_alloc_context();
    // set some encoder settings here
    encoderCtx_->bit_rate = atoi(args_["bitrate"].c_str());
    // emit one intra frame every gop_size frames
    encoderCtx_->gop_size = 15;
    encoderCtx_->max_b_frames = 0;
    encoderCtx_->rtp_payload_size = 0; // Target GOB length
    // resolution must be a multiple of two
    if (args_["width"].empty())
        encoderCtx_->width = inputDecoderCtx_->width;
    else
        encoderCtx_->width = atoi(args_["width"].c_str());

    if (args_["height"].empty())
        encoderCtx_->height = inputDecoderCtx_->height;
    else
        encoderCtx_->height = atoi(args_["height"].c_str());

    // fps
    encoderCtx_->time_base = (AVRational){1, 30};
    encoderCtx_->pix_fmt = PIX_FMT_YUV420P;
    // Fri Jul 22 11:37:59 EDT 2011:tmatth:XXX: DON'T set this, we want our
    // pps and sps to be sent in-band for RTP
    // This is to place global headers in extradata instead of every keyframe.
    // encoderCtx_->flags |= CODEC_FLAG_GLOBAL_HEADER;
}

void VideoSendThread::setup()
{
    int ret;
    av_register_all();
    avdevice_register_all();

    AVInputFormat *file_iformat = 0;
    // it's a v4l device if starting with /dev/video
    if (args_["input"].substr(0, strlen("/dev/video")) == "/dev/video")
    {
        _debug("%s:Using v4l2 format", __PRETTY_FUNCTION__);
        file_iformat = av_find_input_format("video4linux2");
        if (!file_iformat)
        {
            _error("Could not find format video4linux2");
            cleanupAndExit();
        }
    }

    AVDictionary *options = NULL;
    if (!args_["framerate"].empty())
        av_dict_set(&options, "framerate", args_["framerate"].c_str(), 0);
    if (!args_["video_size"].empty())
        av_dict_set(&options, "video_size", args_["video_size"].c_str(), 0);
    if (!args_["channel"].empty())
        av_dict_set(&options, "channel", args_["channel"].c_str(), 0);

    // Open video file
    if (avformat_open_input(&inputCtx_, args_["input"].c_str(), file_iformat, &options) != 0)
    {
        _error("Could not open input file %s", args_["input"].c_str());
        cleanupAndExit();
    }

    // retrieve stream information
    if (av_find_stream_info(inputCtx_) < 0)
    {
        _error("Could not find stream info!");
        cleanupAndExit();
    }

    // find the first video stream from the input
    unsigned i;
    for (i = 0; i < inputCtx_->nb_streams; i++)
    {
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex_ = i;
            break;
        }
    }
    if (videoStreamIndex_ == -1)
    {
        _error("%s:Could not find video stream!", __PRETTY_FUNCTION__);
        cleanupAndExit();
    }

    // Get a pointer to the codec context for the video stream
    inputDecoderCtx_ = inputCtx_->streams[videoStreamIndex_]->codec;

    // find the decoder for the video stream
    AVCodec *inputDecoder = avcodec_find_decoder(inputDecoderCtx_->codec_id);
    if (inputDecoder == NULL)
    {
        _error("%s:Unsupported codec!", __PRETTY_FUNCTION__);
        cleanupAndExit();
    }

    // open codec
    Manager::instance().avcodecLock();
    ret = avcodec_open(inputDecoderCtx_, inputDecoder);
    Manager::instance().avcodecUnlock();
    if (ret < 0)
    {
        _error("%s:Could not open codec!", __PRETTY_FUNCTION__);
        cleanupAndExit();
    }

    outputCtx_ = avformat_alloc_context();

    AVOutputFormat *file_oformat = av_guess_format("rtp", args_["destination"].c_str(), NULL);
    if (!file_oformat)
    {
        _error("Unable to find a suitable output format for %s",
            args_["destination"].c_str());
        cleanupAndExit();
    }
    outputCtx_->oformat = file_oformat;
    strncpy(outputCtx_->filename, args_["destination"].c_str(),
            sizeof outputCtx_->filename);

    AVCodec *encoder = 0;
    /* find the video encoder */
    encoder = avcodec_find_encoder_by_name(args_["codec"].c_str());
    if (encoder == 0)
    {
        _error("%s:Encoder not found!", __PRETTY_FUNCTION__);
        cleanupAndExit();
    }

    prepareEncoderContext();

    /* let x264 preset override our encoder settings */
    if (args_["codec"] == "libx264")
        forcePresetX264();

    scaledPicture_ = avcodec_alloc_frame();

    // open encoder
    Manager::instance().avcodecLock();
    ret = avcodec_open(encoderCtx_, encoder);
    Manager::instance().avcodecUnlock();
    if (ret < 0)
    {
        _error("%s:Could not open encoder!", __PRETTY_FUNCTION__);
        cleanupAndExit();
    }

    // add video stream to outputformat context
    videoStream_ = av_new_stream(outputCtx_, 0);
    if (videoStream_ == 0)
    {
        _error("%s:Could not alloc stream!", __PRETTY_FUNCTION__);
        cleanupAndExit();
    }
    videoStream_->codec = encoderCtx_;

    // open the output file, if needed
    if (!(file_oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&outputCtx_->pb, outputCtx_->filename, AVIO_FLAG_WRITE) < 0)
        {
            _error("%s:Could not open \"%s\"!", outputCtx_->filename);
            cleanupAndExit();
        }
    }
    else
        _debug("No need to open \"%s\"", outputCtx_->filename);

    av_dump_format(outputCtx_, 0, outputCtx_->filename, 1);
    print_and_save_sdp();

    // write the stream header, if any
    if (avformat_write_header(outputCtx_, NULL) < 0)
    {
        _error("%s:Could not write header for output file (incorrect codec "
            "parameters ?)", __PRETTY_FUNCTION__);
        cleanupAndExit();
    }

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

void VideoSendThread::cleanupAndExit()
{
    cleanup();
    exit();
}

void VideoSendThread::cleanup()
{
    _debug("%s", __PRETTY_FUNCTION__);
    // make sure no one is waiting for the SDP which will never come if we've
    // error'd out
    sdpReady_.signal();
    // write the trailer, if any.  the trailer must be written
    // before you close the CodecContexts open when you wrote the
    // header; otherwise write_trailer may try to use memory that
    // was freed on av_codec_close()
    if (outputCtx_ and outputCtx_->priv_data)
        av_write_trailer(outputCtx_);

    if (scaledPictureBuf_)
        av_free(scaledPictureBuf_);
    if (outbuf_)
        av_free(outbuf_);

    // free the scaled frame
    if (scaledPicture_)
        av_free(scaledPicture_);
    // free the YUV frame
    if (rawFrame_)
        av_free(rawFrame_);

    // close the codecs
    Manager::instance().avcodecLock();
    if (encoderCtx_)
    {
        avcodec_close(encoderCtx_);
        av_freep(&encoderCtx_);
    }

    // doesn't need to be freed, we didn't use avcodec_alloc_context
    if (inputDecoderCtx_)
        avcodec_close(inputDecoderCtx_);
    Manager::instance().avcodecUnlock();

    // close the video file
    if (inputCtx_)
        av_close_input_file(inputCtx_);
}

SwsContext * VideoSendThread::createScalingContext()
{
    // Create scaling context
    SwsContext *imgConvertCtx = sws_getContext(inputDecoderCtx_->width,
            inputDecoderCtx_->height, inputDecoderCtx_->pix_fmt, encoderCtx_->width,
            encoderCtx_->height, encoderCtx_->pix_fmt, SWS_BICUBIC,
            NULL, NULL, NULL);
    if (imgConvertCtx == 0)
    {
        _error("%s:Cannot init the conversion context!", __PRETTY_FUNCTION__);
        cleanupAndExit();
    }
    return imgConvertCtx;
}

VideoSendThread::VideoSendThread(const std::map<std::string, std::string> &args) :
    args_(args),
    scaledPictureBuf_(0),
    outbuf_(0),
    inputDecoderCtx_(0),
    rawFrame_(0),
    scaledPicture_(0),
    videoStreamIndex_(-1),
    outbufSize_(0),
    encoderCtx_(0),
    videoStream_(0),
    inputCtx_(0),
    outputCtx_(0),
    sdp_("")
{
    test_source_ = (args_["input"] == "SFLTEST");
    setCancel(cancelImmediate);
}

void VideoSendThread::run()
{
    setup();
    AVPacket inpacket;
    int frameFinished;
    int64_t frameNumber = 0;
    SwsContext *imgConvertCtx = 0;
    int ret, encodedSize;

    if (!test_source_)
        imgConvertCtx = createScalingContext();

    for (;;)
    {
        if (!test_source_) {

            if (av_read_frame(inputCtx_, &inpacket) < 0)
                break;

            // is this a packet from the video stream?
            if (inpacket.stream_index != videoStreamIndex_)
                goto next_packet;

            // decode video frame from camera
            avcodec_decode_video2(inputDecoderCtx_, rawFrame_, &frameFinished, &inpacket);
            if (!frameFinished)
                goto next_packet;

            sws_scale(imgConvertCtx, rawFrame_->data, rawFrame_->linesize,
                    0, inputDecoderCtx_->height, scaledPicture_->data,
                    scaledPicture_->linesize);

        } else {
            const AVPixFmtDescriptor *pixdesc = &av_pix_fmt_descriptors[encoderCtx_->pix_fmt];
            int components = pixdesc->nb_components;
            int planes = 0;
            for (int i = 0; i < components; i++)
                if (pixdesc->comp[i].plane > planes)
                    planes = pixdesc->comp[i].plane;
            planes++;

            int i = frameNumber;
            const unsigned pitch = scaledPicture_->linesize[0];

            for(int y = 0; y < encoderCtx_->height; y++)
                for(unsigned x = 0; x < pitch; x++)
                    scaledPicture_->data[0][y * pitch + x] = x + y + i * planes;
        }

        // Set presentation timestamp on our scaled frame before encoding it
        scaledPicture_->pts = frameNumber++;

        encodedSize = avcodec_encode_video(encoderCtx_,
                outbuf_, outbufSize_, scaledPicture_);

        if (encodedSize <= 0)
            goto next_packet;

        AVPacket opkt;
        av_init_packet(&opkt);

        opkt.data = outbuf_;
        opkt.size = encodedSize;

        // rescale pts from encoded video framerate to rtp
        // clock rate
        if (static_cast<unsigned>(encoderCtx_->coded_frame->pts) !=
                AV_NOPTS_VALUE)
            opkt.pts = av_rescale_q(encoderCtx_->coded_frame->pts,
                    encoderCtx_->time_base, videoStream_->time_base);
        else
            opkt.pts = 0;

        // is it a key frame?
        if (encoderCtx_->coded_frame->key_frame)
            opkt.flags |= AV_PKT_FLAG_KEY;
        opkt.stream_index = videoStream_->index;

        // write the compressed frame in the media file
        ret = av_interleaved_write_frame(outputCtx_, &opkt);
        if (ret < 0)
        {
            print_error("av_interleaved_write_frame() error", ret);
            cleanupAndExit();
        }
        av_free_packet(&opkt);

        // free the packet that was allocated by av_read_frame
next_packet:
        av_free_packet(&inpacket);
        yield();
    }
}

void VideoSendThread::final()
{
    cleanup();
}

VideoSendThread::~VideoSendThread()
{
    ost::Thread::terminate();
}

} // end namespace sfl_video
