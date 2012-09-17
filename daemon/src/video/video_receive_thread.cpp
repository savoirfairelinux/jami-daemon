/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#include "video_receive_thread.h"
#include "dbus/video_controls.h"
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
#include <map>
#include <ctime>
#include <cstdlib>
#include <cstdio> // for remove()
#include <fstream>

#include "manager.h"

static const enum PixelFormat VIDEO_RGB_FORMAT = PIX_FMT_BGRA;

namespace sfl_video {

using std::string;

namespace { // anonymous namespace

int getBufferSize(int width, int height, int format)
{
    enum PixelFormat fmt = (enum PixelFormat) format;
    // determine required buffer size and allocate buffer
    return sizeof(unsigned char) * avpicture_get_size(fmt, width, height);
}

string openTemp(string path, std::ofstream& os)
{
    path += "/";
    // POSIX the mktemp family of functions requires names to end with 6 x's
    const char * const X_SUFFIX = "XXXXXX";

    path += X_SUFFIX;
    std::vector<char> dst_path(path.begin(), path.end());
    dst_path.push_back('\0');
    for (int fd = -1; fd == -1; ) {
        fd = mkstemp(&dst_path[0]);
        if (fd != -1) {
            path.assign(dst_path.begin(), dst_path.end() - 1);
            os.open(path.c_str(), std::ios_base::trunc | std::ios_base::out);
            close(fd);
        }
    }
    return path;
}
} // end anonymous namespace

void VideoReceiveThread::loadSDP()
{
    EXIT_IF_FAIL(not args_["receiving_sdp"].empty(), "Cannot load empty SDP");

    std::ofstream os;
    sdpFilename_ = openTemp("/tmp", os);
    os << args_["receiving_sdp"];
    DEBUG("loaded SDP\n%s", args_["receiving_sdp"].c_str());

    os.close();
}

void VideoReceiveThread::openDecoder()
{
    if (decoderCtx_)
        avcodec_close(decoderCtx_);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    int ret = avcodec_open(decoderCtx_, inputDecoder_);
#else
    int ret = avcodec_open2(decoderCtx_, inputDecoder_, NULL);
#endif
    EXIT_IF_FAIL(ret == 0, "Could not open codec");
}

// We do this setup here instead of the constructor because we don't want the
// main thread to block while this executes, so it happens in the video thread.
void VideoReceiveThread::setup()
{
    dstWidth_ = atoi(args_["width"].c_str());
    dstHeight_ = atoi(args_["height"].c_str());

    std::string format_str;
    std::string input;
    if (args_["input"].empty()) {
        loadSDP();
        format_str = "sdp";
        input = sdpFilename_;
    } else if (args_["input"].substr(0, strlen("/dev/video")) == "/dev/video") {
        // it's a v4l device if starting with /dev/video
        // FIXME: This is not a robust way of checking if we mean to use a
        // v4l2 device
        format_str = "video4linux2";
        input = args_["input"];
    }

    DEBUG("Using %s format", format_str.c_str());
    AVInputFormat *file_iformat = av_find_input_format(format_str.c_str());
    EXIT_IF_FAIL(file_iformat, "Could not find format \"%s\"", format_str.c_str());

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
    int ret = avformat_open_input(&inputCtx_, input.c_str(), file_iformat, options ? &options : NULL);
    EXIT_IF_FAIL(ret == 0, "Could not open input \"%s\"", input.c_str());

    DEBUG("Finding stream info");
    if (requestKeyFrameCallback_)
        requestKeyFrameCallback_(id_);

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    ret = av_find_stream_info(inputCtx_);
#else
    ret = avformat_find_stream_info(inputCtx_, options ? &options : NULL);
#endif
    EXIT_IF_FAIL(ret >= 0, "Could not find stream info!");

    // find the first video stream from the input
    for (size_t i = 0; streamIndex_ == -1 && i < inputCtx_->nb_streams; ++i)
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            streamIndex_ = i;

    EXIT_IF_FAIL(streamIndex_ != -1, "Could not find video stream");

    // Get a pointer to the codec context for the video stream
    decoderCtx_ = inputCtx_->streams[streamIndex_]->codec;

    // find the decoder for the video stream
    inputDecoder_ = avcodec_find_decoder(decoderCtx_->codec_id);
    EXIT_IF_FAIL(inputDecoder_, "Unsupported codec");

    openDecoder();

    scaledPicture_ = avcodec_alloc_frame();
    EXIT_IF_FAIL(scaledPicture_, "Could not allocate output frame");

    if (dstWidth_ == 0 and dstHeight_ == 0) {
        dstWidth_ = decoderCtx_->width;
        dstHeight_ = decoderCtx_->height;
    }

    // determine required buffer size and allocate buffer
    bufferSize_ = getBufferSize(dstWidth_, dstHeight_, VIDEO_RGB_FORMAT);

    EXIT_IF_FAIL(sink_.start(), "Cannot start shared memory sink");
    Manager::instance().getVideoControls()->startedDecoding(id_, sink_.openedName(), dstWidth_, dstHeight_);
    DEBUG("shm sink started with size %d, width %d and height %d", bufferSize_, dstWidth_, dstHeight_);
}

void VideoReceiveThread::createScalingContext()
{
    // Create scaling context, no scaling done here
    imgConvertCtx_ = sws_getCachedContext(imgConvertCtx_, decoderCtx_->width,
                                          decoderCtx_->height,
                                          decoderCtx_->pix_fmt, dstWidth_,
                                          dstHeight_, VIDEO_RGB_FORMAT,
                                          SWS_BICUBIC, NULL, NULL, NULL);
    EXIT_IF_FAIL(imgConvertCtx_, "Cannot init the conversion context!");
}

// This callback is used by libav internally to break out of blocking calls
int VideoReceiveThread::interruptCb(void *ctx)
{
    VideoReceiveThread *context = static_cast<VideoReceiveThread*>(ctx);
    return not context->threadRunning_;
}

VideoReceiveThread::VideoReceiveThread(const std::string &id, const std::map<string, string> &args) :
    args_(args), frameNumber_(0), inputDecoder_(0), decoderCtx_(0), rawFrame_(0),
    scaledPicture_(0), streamIndex_(-1), inputCtx_(0), imgConvertCtx_(0),
    dstWidth_(0), dstHeight_(0), sink_(), threadRunning_(false),
    sdpFilename_(), bufferSize_(0), id_(id), interruptCb_(), requestKeyFrameCallback_(0)
{
    interruptCb_.callback = interruptCb;
    interruptCb_.opaque = this;
}

void VideoReceiveThread::start()
{
    threadRunning_ = true;
    ost::Thread::start();
}


/// Copies and scales our rendered frame to the buffer pointed to by data
void VideoReceiveThread::fill_buffer(void *data)
{
    avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
                   static_cast<uint8_t *>(data),
                   VIDEO_RGB_FORMAT,
                   dstWidth_,
                   dstHeight_);

    sws_scale(imgConvertCtx_,
            rawFrame_->data,
            rawFrame_->linesize,
            0,
            decoderCtx_->height,
            scaledPicture_->data,
            scaledPicture_->linesize);
}

void VideoReceiveThread::run()
{
    setup();

    createScalingContext();
    const Callback cb(&VideoReceiveThread::fill_buffer);
    AVFrame rawFrame;
    rawFrame_ = &rawFrame;

    while (threadRunning_) {
        AVPacket inpacket;

        int ret = 0;
        if ((ret = av_read_frame(inputCtx_, &inpacket)) < 0) {
            ERROR("Couldn't read frame: %s\n", strerror(ret));
            break;
        }
        // Guarantee that we free the packet every iteration
        PacketHandle inpacket_handle(inpacket);
        avcodec_get_frame_defaults(rawFrame_);

        // is this a packet from the video stream?
        if (inpacket.stream_index == streamIndex_) {
            int frameFinished = 0;
            const int len = avcodec_decode_video2(decoderCtx_, rawFrame_, &frameFinished,
                                                  &inpacket);
            if (len <= 0 and requestKeyFrameCallback_) {
                openDecoder();
                requestKeyFrameCallback_(id_);
                ost::Thread::sleep(25 /* ms */);
            }

            // we want our rendering code to be called by the shm_sink,
            // because it manages the shared memory synchronization
            if (frameFinished)
                sink_.render_callback(this, cb, bufferSize_);
        }
        yield();
    }
}

VideoReceiveThread::~VideoReceiveThread()
{
    threadRunning_ = false;
    Manager::instance().getVideoControls()->stoppedDecoding(id_, sink_.openedName());
    // this calls join, which waits for the run() method (in separate thread) to return
    ost::Thread::terminate();

    if (imgConvertCtx_)
        sws_freeContext(imgConvertCtx_);

    if (scaledPicture_)
        av_free(scaledPicture_);

    if (decoderCtx_)
        avcodec_close(decoderCtx_);

    if (streamIndex_ != -1 and inputCtx_) {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
        av_close_input_file(inputCtx_);
#else
        avformat_close_input(&inputCtx_);
#endif
    }
    if (not sdpFilename_.empty() and remove(sdpFilename_.c_str()) != 0)
        ERROR("Could not remove %s", sdpFilename_.c_str());
}

void VideoReceiveThread::setRequestKeyFrameCallback(void (*cb)(const std::string &))
{
    requestKeyFrameCallback_ = cb;
}

void
VideoReceiveThread::addDetails(std::map<std::string, std::string> &details)
{
    if (threadRunning_ and dstWidth_ > 0 and dstHeight_ > 0) {
        details["VIDEO_SHM_PATH"] = sink_.openedName();
        std::ostringstream os;
        os << dstWidth_;
        details["VIDEO_WIDTH"] = os.str();
        os.str("");
        os << dstHeight_;
        details["VIDEO_HEIGHT"] = os.str();
    }
}

} // end namespace sfl_video
