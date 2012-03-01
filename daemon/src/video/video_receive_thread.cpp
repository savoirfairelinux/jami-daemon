/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *
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

#include "video_receive_thread.h"
#include "packet_handle.h"

// libav includes
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

#include <ctime>
#include <cstdlib>

#include "manager.h"
#include "dbus/callmanager.h"
#include "dbus/video_controls.h"
#include "fileutils.h"

static const enum PixelFormat video_rgb_format = PIX_FMT_BGRA;

namespace sfl_video {

using std::map;
using std::string;

namespace { // anonymous namespace

int bufferSize(int width, int height, int format)
{
	enum PixelFormat fmt = (enum PixelFormat) format;
    // determine required buffer size and allocate buffer
    return sizeof(uint8_t) * avpicture_get_size(fmt, width, height);
}

} // end anonymous namespace

void VideoReceiveThread::loadSDP()
{
    assert(not args_["receiving_sdp"].empty());
    // this memory will be released on next call to tmpnam
    std::ofstream os;
    sdpFilename_ = openTemp("/tmp", os);

    os << args_["receiving_sdp"];
    DEBUG("%s:loaded SDP %s", __PRETTY_FUNCTION__,
          args_["receiving_sdp"].c_str());

    os.close();
}

void VideoReceiveThread::setup()
{
    dstWidth_ = atoi(args_["width"].c_str());
    dstHeight_ = atoi(args_["height"].c_str());

    AVInputFormat *file_iformat = 0;

    if (args_["input"].empty()) {
        loadSDP();
        args_["input"] = sdpFilename_;
        file_iformat = av_find_input_format("sdp");
        if (!file_iformat) {
            ERROR("%s:Could not find format \"sdp\"", __PRETTY_FUNCTION__);
            ost::Thread::exit();
        }
    } else if (args_["input"].substr(0, strlen("/dev/video")) == "/dev/video") {
        // it's a v4l device if starting with /dev/video
        // FIXME: This is not the most robust way of checking if we mean to use a
        // v4l device
        DEBUG("Using v4l2 format");
        file_iformat = av_find_input_format("video4linux2");
        if (!file_iformat) {
            ERROR("%s:Could not find format!", __PRETTY_FUNCTION__);
            ost::Thread::exit();
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
    if (avformat_open_input(&inputCtx_, args_["input"].c_str(), file_iformat,
                            &options) != 0) {
        ERROR("%s:Could not open input file \"%s\"", __PRETTY_FUNCTION__,
              args_["input"].c_str());
        ost::Thread::exit();
    }

    // retrieve stream information
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    if (av_find_stream_info(inputCtx_) < 0) {
#else
    if (avformat_find_stream_info(inputCtx_, NULL) < 0) {
#endif
        ERROR("%s:Could not find stream info!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

    // find the first video stream from the input
    for (unsigned i = 0; i < inputCtx_->nb_streams; i++) {
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            break;
        }
    }

    if (videoStreamIndex_ == -1) {
        ERROR("%s:Could not find video stream!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

    // Get a pointer to the codec context for the video stream
    decoderCtx_ = inputCtx_->streams[videoStreamIndex_]->codec;

    // find the decoder for the video stream
    AVCodec *inputDecoder = avcodec_find_decoder(decoderCtx_->codec_id);
    if (inputDecoder == NULL) {
        ERROR("%s:Unsupported codec!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    if (avcodec_open(decoderCtx_, inputDecoder) < 0) {
#else
    if (avcodec_open2(decoderCtx_, inputDecoder, NULL) < 0) {
#endif
        ERROR("%s:Could not open codec!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

    scaledPicture_ = avcodec_alloc_frame();
    if (scaledPicture_ == 0) {
        ERROR("%s:Could not allocated output frame!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

    if (dstWidth_ == 0 and dstHeight_ == 0) {
        dstWidth_ = decoderCtx_->width;
        dstHeight_ = decoderCtx_->height;
    }

    // determine required buffer size and allocate buffer
    videoBufferSize_ = bufferSize(dstWidth_, dstHeight_, video_rgb_format);

    // allocate video frame
    rawFrame_ = avcodec_alloc_frame();
}

void VideoReceiveThread::createScalingContext()
{
    // Create scaling context, no scaling done here
    imgConvertCtx_ = sws_getCachedContext(imgConvertCtx_, decoderCtx_->width,
                                          decoderCtx_->height,
                                          decoderCtx_->pix_fmt, dstWidth_,
                                          dstHeight_, video_rgb_format,
                                          SWS_BICUBIC, NULL, NULL, NULL);
    if (imgConvertCtx_ == 0) {
        ERROR("Cannot init the conversion context!");
        ost::Thread::exit();
    }
}

VideoReceiveThread::VideoReceiveThread(const map<string, string> &args) :
    args_(args),
    frameNumber_(0),
    decoderCtx_(0),
    rawFrame_(0),
    scaledPicture_(0),
    videoStreamIndex_(-1),
    inputCtx_(0),
    imgConvertCtx_(0),
    dstWidth_(-1),
    dstHeight_(-1),
    sdpFilename_()
{
    setCancel(cancelDeferred);
}

void VideoReceiveThread::run()
{
    setup();

    createScalingContext();
    while (not testCancel()) {
        AVPacket inpacket;

        int ret = 0;
        if ((ret = av_read_frame(inputCtx_, &inpacket)) < 0) {
            ERROR("Couldn't read frame : %s\n", perror(ret));
            break;
        }
        PacketHandle inpacket_handle(inpacket);

        // is this a packet from the video stream?
        if (inpacket.stream_index == videoStreamIndex_) {
            int frameFinished;
            avcodec_decode_video2(decoderCtx_, rawFrame_, &frameFinished, &inpacket);
            if (frameFinished) {
                avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
                               targetBuffer_, video_rgb_format, dstWidth_,
                               dstHeight_);

                sws_scale(imgConvertCtx_, rawFrame_->data, rawFrame_->linesize,
                          0, decoderCtx_->height, scaledPicture_->data,
                          scaledPicture_->linesize);

                sharedMemory_.frameUpdatedCallback();
            }
        }
    }
}

VideoReceiveThread::~VideoReceiveThread()
{
    ost::Thread::terminate();
    sharedMemory_.unsubscribe(this);

    if (imgConvertCtx_)
        sws_freeContext(imgConvertCtx_);

    if (scaledPicture_)
        av_free(scaledPicture_);

    if (rawFrame_)
        av_free(rawFrame_);

    if (decoderCtx_)
        avcodec_close(decoderCtx_);

    if (inputCtx_)
        av_close_input_file(inputCtx_);
}
} // end namespace sfl_video
