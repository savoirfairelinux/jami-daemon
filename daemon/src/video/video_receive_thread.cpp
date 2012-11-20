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
#include "check.h"
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
#include <map>

#include "manager.h"

static const enum PixelFormat VIDEO_RGB_FORMAT = PIX_FMT_BGRA;

namespace sfl_video {

using std::string;

namespace { // anonymous namespace

int getBufferSize(int width, int height, int format)
{
    enum PixelFormat fmt = (enum PixelFormat) format;
    // determine required buffer size and allocate buffer
    return avpicture_get_size(fmt, width, height);
}

int readFunction(void *opaque, uint8_t *buf, int buf_size)
{
    std::istream &is = *static_cast<std::istream*>(opaque);
    is.read(reinterpret_cast<char*>(buf), buf_size);
    return is.gcount();
}

const int SDP_BUFFER_SIZE = 8192;

} // end anonymous namespace

void VideoReceiveThread::openDecoder()
{
    if (decoderCtx_)
        avcodec_close(decoderCtx_);
    inputDecoder_ = avcodec_find_decoder(decoderCtx_->codec_id);
    decoderCtx_->thread_count = 1;
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

    const std::string SDP_FILENAME = "dummyFilename";
    std::string format_str;
    std::string input;
    if (args_["input"].empty()) {
        format_str = "sdp";
        input = SDP_FILENAME;
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
    if (input == SDP_FILENAME) {
        EXIT_IF_FAIL(not stream_.str().empty(), "No SDP loaded");
        inputCtx_->pb = avioContext_.get();
    }
    int ret = avformat_open_input(&inputCtx_, input.c_str(), file_iformat, options ? &options : NULL);

    if (ret < 0) {
        if (options)
            av_dict_free(&options);
        ERROR("Could not open input \"%s\"", input.c_str());
        return;
    }

    DEBUG("Finding stream info");
    if (requestKeyFrameCallback_)
        requestKeyFrameCallback_(id_);

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    ret = av_find_stream_info(inputCtx_);
#else
    ret = avformat_find_stream_info(inputCtx_, options ? &options : NULL);
#endif
    if (options)
        av_dict_free(&options);
    if (ret < 0) {
        // workaround for this bug:
        // http://patches.libav.org/patch/22541/
        if (ret == -1)
            ret = AVERROR_INVALIDDATA;
        char errBuf[64] = {0};
        // print nothing for unknown errors
        if (av_strerror(ret, errBuf, sizeof errBuf) < 0)
            errBuf[0] = '\0';
        // always fail here
        EXIT_IF_FAIL(false, "Could not find stream info: %s", errBuf);
    }

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
    if (!imgConvertCtx_) {
        ERROR("Cannot init the conversion context!");
        pthread_exit(NULL);
    }
}

// This callback is used by libav internally to break out of blocking calls
int VideoReceiveThread::interruptCb(void *ctx)
{
    VideoReceiveThread *context = static_cast<VideoReceiveThread*>(ctx);
    return not context->threadRunning_;
}

VideoReceiveThread::VideoReceiveThread(const std::string &id, const std::map<string, string> &args) :
    args_(args), inputDecoder_(0), decoderCtx_(0), rawFrame_(0),
    scaledPicture_(0), streamIndex_(-1), inputCtx_(0), imgConvertCtx_(0),
    dstWidth_(0), dstHeight_(0), sink_(), threadRunning_(false),
    bufferSize_(0), id_(id), interruptCb_(), requestKeyFrameCallback_(0),
    sdpBuffer_(reinterpret_cast<unsigned char*>(av_malloc(SDP_BUFFER_SIZE)), &av_free),
    stream_(args_["receiving_sdp"]),
    avioContext_(avio_alloc_context(sdpBuffer_.get(), SDP_BUFFER_SIZE, 0,
                                    reinterpret_cast<void*>(static_cast<std::istream*>(&stream_)),
                                    &readFunction, 0, 0), &av_free),
    thread_()
{
    interruptCb_.callback = interruptCb;
    interruptCb_.opaque = this;
}

void VideoReceiveThread::start()
{
    threadRunning_ = true;
    pthread_create(&thread_, NULL, &runCallback, this);
}


void *VideoReceiveThread::runCallback(void *data)
{
    VideoReceiveThread *context = static_cast<VideoReceiveThread*>(data);
    context->run();
    return NULL;
}

/// Copies and scales our rendered frame to the buffer pointed to by data
void VideoReceiveThread::fill_buffer(void *data)
{
    avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
                   static_cast<uint8_t *>(data),
                   VIDEO_RGB_FORMAT,
                   dstWidth_,
                   dstHeight_);

    createScalingContext();

    sws_scale(imgConvertCtx_,
            rawFrame_->data,
            rawFrame_->linesize,
            0,
            decoderCtx_->height,
            scaledPicture_->data,
            scaledPicture_->linesize);
}

struct VideoRxContextHandle {
    VideoRxContextHandle(VideoReceiveThread &rx) : rx_(rx) {}

    ~VideoRxContextHandle()
    {
        if (rx_.imgConvertCtx_)
            sws_freeContext(rx_.imgConvertCtx_);

        if (rx_.scaledPicture_)
            av_free(rx_.scaledPicture_);

        if (rx_.decoderCtx_)
            avcodec_close(rx_.decoderCtx_);

        if (rx_.inputCtx_ and rx_.inputCtx_->nb_streams > 0) {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
            av_close_input_file(rx_.inputCtx_);
#else
            avformat_close_input(&rx_.inputCtx_);
#endif
        }
    }
    VideoReceiveThread &rx_;
};

void VideoReceiveThread::run()
{
    VideoRxContextHandle handle(*this);
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
            threadRunning_ = false;
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
            }

            // we want our rendering code to be called by the shm_sink,
            // because it manages the shared memory synchronization
            if (frameFinished)
                sink_.render_callback(this, cb, bufferSize_);
        }
    }
}

VideoReceiveThread::~VideoReceiveThread()
{
    threadRunning_ = false;
    Manager::instance().getVideoControls()->stoppedDecoding(id_, sink_.openedName());
    // waits for the run() method (in separate thread) to return
    pthread_join(thread_, NULL);
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
