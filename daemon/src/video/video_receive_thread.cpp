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
#include <map>
#include <ctime>
#include <cstdlib>
#include <fstream>

#include "manager.h"

static const enum PixelFormat VIDEO_RGB_FORMAT = PIX_FMT_BGRA;

namespace sfl_video {

using std::string;

namespace { // anonymous namespace

// FIXME: this is an ugly hack to generate a unique filename for shm_open
// While there's a good chance that /dev/shm/sfl_XXXX will not
// be in use we need a better way of doing this.

std::string createTempFileName()
{
    std::string path;
    const char SHM_PATH[] = "/dev/shm";
    int i;
    for (i = 0; path.empty() and i < TMP_MAX; ++i) {
        char *fname = tempnam(SHM_PATH, "sfl_");
        // if no file with name path exists, the path is usable
        if (access(fname, F_OK))
            path = std::string(fname + sizeof(SHM_PATH));
        free(fname);
    }
    if (i == TMP_MAX)
        throw std::runtime_error("Could not generate unique filename");

    DEBUG("Created path %s", path.c_str());
    return path;
}

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
    assert(not args_["receiving_sdp"].empty());

    std::ofstream os;
    sdpFilename_ = openTemp("/tmp", os);
    os << args_["receiving_sdp"];
    DEBUG("loaded SDP\n%s", args_["receiving_sdp"].c_str());

    os.close();
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
    RETURN_IF_FAIL(file_iformat, "Could not find format \"%s\"", format_str.c_str());

    AVDictionary *options = NULL;
    if (!args_["framerate"].empty())
        av_dict_set(&options, "framerate", args_["framerate"].c_str(), 0);
    if (!args_["video_size"].empty())
        av_dict_set(&options, "video_size", args_["video_size"].c_str(), 0);
    if (!args_["channel"].empty())
        av_dict_set(&options, "channel", args_["channel"].c_str(), 0);

    // Open video file
    DEBUG("Opening input");
    int ret = avformat_open_input(&inputCtx_, input.c_str(), file_iformat, options ? &options : NULL);
    RETURN_IF_FAIL(ret == 0, "Could not open input \"%s\"", input.c_str());

    DEBUG("Finding stream info");
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    ret = av_find_stream_info(inputCtx_);
#else
    ret = avformat_find_stream_info(inputCtx_, options ? &options : NULL);
#endif
    RETURN_IF_FAIL(ret >= 0, "Could not find stream info!");

    // find the first video stream from the input
    streamIndex_ = -1;
    for (size_t i = 0; streamIndex_ == -1 && i < inputCtx_->nb_streams; ++i)
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            streamIndex_ = i;

    RETURN_IF_FAIL(streamIndex_ != -1, "Could not find video stream");

    // Get a pointer to the codec context for the video stream
    decoderCtx_ = inputCtx_->streams[streamIndex_]->codec;

    // find the decoder for the video stream
    AVCodec *inputDecoder = avcodec_find_decoder(decoderCtx_->codec_id);
    RETURN_IF_FAIL(inputDecoder, "Unsupported codec");

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    ret = avcodec_open(decoderCtx_, inputDecoder);
#else
    ret = avcodec_open2(decoderCtx_, inputDecoder, NULL);
#endif
    RETURN_IF_FAIL(ret == 0, "Could not open codec");

    scaledPicture_ = avcodec_alloc_frame();
    RETURN_IF_FAIL(scaledPicture_, "Could not allocate output frame");

    if (dstWidth_ == 0 and dstHeight_ == 0) {
        dstWidth_ = decoderCtx_->width;
        dstHeight_ = decoderCtx_->height;
    }

    // determine required buffer size and allocate buffer
    outBuffer_.resize(getBufferSize(dstWidth_, dstHeight_, VIDEO_RGB_FORMAT));

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
    RETURN_IF_FAIL(imgConvertCtx_, "Cannot init the conversion context!");
}

VideoReceiveThread::VideoReceiveThread(const std::map<string, string> &args) :
    args_(args), frameNumber_(0), decoderCtx_(0), rawFrame_(0),
    scaledPicture_(0), streamIndex_(-1), inputCtx_(0), imgConvertCtx_(0),
    dstWidth_(0), dstHeight_(0), sink_(createTempFileName()),
    receiving_(false), sdpFilename_(), outBuffer_()
{}

void VideoReceiveThread::run()
{
    setup();
    RETURN_IF_FAIL(sink_.start(), "Cannot start shared memory sink");

    createScalingContext();
    receiving_ = true;
    while (receiving_) {
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
                               &(*outBuffer_.begin()), VIDEO_RGB_FORMAT,
                               dstWidth_, dstHeight_);

                sws_scale(imgConvertCtx_, rawFrame_->data, rawFrame_->linesize,
                          0, decoderCtx_->height, scaledPicture_->data,
                          scaledPicture_->linesize);

                sink_.render(outBuffer_);
            }
        }
        yield();
    }
}

VideoReceiveThread::~VideoReceiveThread()
{
    receiving_ = false;
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
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
        av_close_input_file(inputCtx_);
#else
        avformat_close_input(&inputCtx_);
#endif
}
} // end namespace sfl_video
