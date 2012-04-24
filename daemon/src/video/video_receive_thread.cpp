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
#include "check.h"

// libav includes
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

#include <stdexcept>
#include <ctime>
#include <cstdlib>

#include "manager.h"
#include "dbus/video_controls.h"
#include "shared_memory.h"

static const enum PixelFormat VIDEO_RGB_FORMAT = PIX_FMT_BGRA;

namespace sfl_video {

using std::map;
using std::string;

namespace { // anonymous namespace

int getBufferSize(int width, int height, int format)
{
	enum PixelFormat fmt = (enum PixelFormat) format;
    // determine required buffer size and allocate buffer
    return sizeof(uint8_t) * avpicture_get_size(fmt, width, height);
}

} // end anonymous namespace

void VideoReceiveThread::loadSDP()
{
    assert(not args_["receiving_sdp"].empty());

    std::ofstream os;
    os << args_["receiving_sdp"];
    DEBUG("%s:loaded SDP %s", __PRETTY_FUNCTION__,
          args_["receiving_sdp"].c_str());

    os.close();
}

// We do this setup here instead of the constructor because we don't want the
// main thread to block while this executes, so it happens in the video thread.
void VideoReceiveThread::setup()
{
    dstWidth_ = atoi(args_["width"].c_str());
    dstHeight_ = atoi(args_["height"].c_str());

    AVInputFormat *file_iformat = 0;

    std::string format_str;
    if (args_["input"].empty()) {
        loadSDP();
        format_str = "sdp";
    } else if (args_["input"].substr(0, strlen("/dev/video")) == "/dev/video") {
        // it's a v4l device if starting with /dev/video
        // FIXME: This is not a robust way of checking if we mean to use a
        // v4l2 device
        format_str = "video4linux2";
    }

    DEBUG("Using %s format", format_str.c_str());
    file_iformat = av_find_input_format(format_str.c_str());
    CHECK(file_iformat, "Could not find format \"%s\"", format_str.c_str());

    AVDictionary *options = NULL;
    if (!args_["framerate"].empty())
        av_dict_set(&options, "framerate", args_["framerate"].c_str(), 0);
    if (!args_["video_size"].empty())
        av_dict_set(&options, "video_size", args_["video_size"].c_str(), 0);
    if (!args_["channel"].empty())
        av_dict_set(&options, "channel", args_["channel"].c_str(), 0);

    // Open video file
    int ret = avformat_open_input(&inputCtx_, args_["input"].c_str(), file_iformat,
                            &options);
    CHECK(ret == 0, "Could not open input \"%s\"", args_["input"].c_str());

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    ret = av_find_stream_info(inputCtx_);
#else
    ret = avformat_find_stream_info(inputCtx_, NULL);
#endif
    CHECK(ret >= 0, "Could not find stream info!");

    // find the first video stream from the input
    streamIndex_ = -1;
    for (size_t i = 0; streamIndex_ == -1 && i < inputCtx_->nb_streams; ++i)
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            streamIndex_ = i;

    CHECK(streamIndex_ != -1, "Could not find video stream");

    // Get a pointer to the codec context for the video stream
    decoderCtx_ = inputCtx_->streams[streamIndex_]->codec;

    // find the decoder for the video stream
    AVCodec *inputDecoder = avcodec_find_decoder(decoderCtx_->codec_id);
    CHECK(inputDecoder, "Unsupported codec");

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    ret = avcodec_open(decoderCtx_, inputDecoder);
#else
    ret = avcodec_open2(decoderCtx_, inputDecoder, NULL);
#endif
    CHECK(ret >= 0, "Could not open codec");

    scaledPicture_ = avcodec_alloc_frame();
    CHECK(scaledPicture_, "Could not allocate output frame");

    if (dstWidth_ == 0 and dstHeight_ == 0) {
        dstWidth_ = decoderCtx_->width;
        dstHeight_ = decoderCtx_->height;
    }

    // determine required buffer size and allocate buffer
    const int bufferSize = getBufferSize(dstWidth_, dstHeight_, VIDEO_RGB_FORMAT);
    try {
        sharedMemory_.allocateBuffer(dstWidth_, dstHeight_, bufferSize);
    } catch (const std::runtime_error &e) {
        CHECK(false, "%s", e.what());
    }

    // allocate video frame
    rawFrame_ = avcodec_alloc_frame();
}

void VideoReceiveThread::createScalingContext()
{
    // Create scaling context, no scaling done here
    imgConvertCtx_ = sws_getCachedContext(imgConvertCtx_, decoderCtx_->width,
                                          decoderCtx_->height,
                                          decoderCtx_->pix_fmt, dstWidth_,
                                          dstHeight_, VIDEO_RGB_FORMAT,
                                          SWS_BICUBIC, NULL, NULL, NULL);
    CHECK(imgConvertCtx_, "Cannot init the conversion context!");
}

VideoReceiveThread::VideoReceiveThread(const map<string, string> &args,
                                       sfl_video::SharedMemory &handle) :
    args_(args), frameNumber_(0), decoderCtx_(0), rawFrame_(0),
    scaledPicture_(0), streamIndex_(-1), inputCtx_(0), imgConvertCtx_(0),
    dstWidth_(-1), dstHeight_(-1), sharedMemory_(handle)
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
            ERROR("Couldn't read frame : %s\n", strerror(ret));
            break;
        }
        PacketHandle inpacket_handle(inpacket);

        // is this a packet from the video stream?
        if (inpacket.stream_index == streamIndex_) {
            int frameFinished;
            avcodec_decode_video2(decoderCtx_, rawFrame_, &frameFinished,
                                  &inpacket);
            if (frameFinished) {
                avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
                               sharedMemory_.getTargetBuffer(),
                               VIDEO_RGB_FORMAT, dstWidth_, dstHeight_);

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
