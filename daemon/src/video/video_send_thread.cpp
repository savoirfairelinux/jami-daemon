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
#include "socket_pair.h"
#include "client/video_controls.h"
#include "check.h"

#include <map>
#include "manager.h"

namespace sfl_video {

using std::string;

void VideoSendThread::setup()
{
    const char *enc_name = args_["codec"].c_str();
    // it's a v4l device if starting with /dev/video
    static const char * const V4L_PATH = "/dev/video";

	std::string format_str;
	std::string input = args_["input"];

	/* Decoder setup */

    if (args_["input"].find(V4L_PATH) != std::string::npos) {
        DEBUG("Using v4l2 format");
        format_str = "video4linux2";
    }

	if (!args_["framerate"].empty())
		videoDecoder_->setOption("framerate", args_["framerate"].c_str());
    if (!args_["video_size"].empty())
		videoDecoder_->setOption("video_size", args_["video_size"].c_str());
    if (!args_["channel"].empty())
		videoDecoder_->setOption("channel", args_["channel"].c_str());

	videoDecoder_->setInterruptCallback(interruptCb, this);

	/* Open the decoder and start the stream */

	EXIT_IF_FAIL(videoDecoder_->openInput(input, format_str) >= 0,
				 "Could not open input \"%s\"", input.c_str());
    EXIT_IF_FAIL(sink_.start(), "Cannot start shared memory sink");

	/* Data available, finish the decoding */
	EXIT_IF_FAIL(!videoDecoder_->setupFromVideoData(),
				 "decoder IO startup failed");
	bufferSize_ = VideoDecoder::getBufferSize(PIX_FMT_BGRA,
											  videoDecoder_->getWidth(),
											  videoDecoder_->getHeight());
	EXIT_IF_FAIL(bufferSize_ > 0, "Incorrect buffer size for decoding");

    Manager::instance().getVideoControls()->startedDecoding(id_,
															sink_.openedName(),
															videoDecoder_->getWidth(),
															videoDecoder_->getHeight());
    DEBUG("TX: shm sink started with size %d, width %d and height %d", bufferSize_,
		  videoDecoder_->getWidth(), videoDecoder_->getHeight());

	/* Encoder setup */

	videoEncoder_->setInterruptCallback(interruptCb, this);

	videoEncoder_->setOption("bitrate", args_["bitrate"].c_str());
	if (!args_["width"].empty()) {
		videoEncoder_->setOption("width", args_["width"].c_str());
	} else {
		char buf[11];
		sprintf(buf, "%10d", videoDecoder_->getWidth());
		videoEncoder_->setOption("width", buf);
	}

	if (!args_["height"].empty()) {
		videoEncoder_->setOption("height", args_["height"].c_str());
	} else {
		char buf[11];
		sprintf(buf, "%10d", videoDecoder_->getHeight());
		videoEncoder_->setOption("height", buf);
	}

	if (!args_["framerate"].empty())
		videoEncoder_->setOption("framerate", args_["framerate"].c_str());

	if (!args_["parameters"].empty())
		videoEncoder_->setOption("parameters", args_["parameters"].c_str());

	// write the stream header, if any
    if (!args_["payload_type"].empty()) {
        DEBUG("Writing stream header for payload type %s",
			  args_["payload_type"].c_str());
        videoEncoder_->setOption("payload_type", args_["payload_type"].c_str());
    }

	EXIT_IF_FAIL(!videoEncoder_->openOutput(enc_name, "rtp",
										   args_["destination"].c_str(), NULL),
				 "encoder openOutput() failed with input \"%s\"",
				 input.c_str());

	videoEncoder_->setIOContext(muxContext_);
	EXIT_IF_FAIL(!videoEncoder_->startIO(), "encoder start failed");
	videoEncoder_->print_sdp(sdp_);
}

// This callback is used by libav internally to break out of blocking calls
int VideoSendThread::interruptCb(void *ctx)
{
    VideoSendThread *context = static_cast<VideoSendThread*>(ctx);
    return not context->threadRunning_;
}

VideoSendThread::VideoSendThread(const std::string &id, const std::map<string, string> &args) :
    args_(args),
	videoDecoder_(0),
	videoEncoder_(0),
	scaledInputBuffer_(0),
    encoderBuffer_(0),
    encoderBufferSize_(0),
    sdp_(),
    sink_(),
    bufferSize_(0),
    id_(id),
    interruptCb_(),
    threadRunning_(false),
    forceKeyFrame_(0),
    thread_(0),
    frameNumber_(0),
    muxContext_(0)
{
    interruptCb_.callback = interruptCb;
    interruptCb_.opaque = this;
}

void VideoSendThread::addIOContext(SocketPair &socketPair)
{
	muxContext_ = socketPair.getIOContext();
}

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
	videoDecoder_ = new VideoDecoder();
	videoEncoder_ = new VideoEncoder();

	setup();

    while (threadRunning_) {
        if (captureFrame()) {
            renderFrame();
            encodeAndSendVideo();
        }
	}

	if (muxContext_)
		delete muxContext_;

	delete videoDecoder_;
	delete videoEncoder_;
}

/// Copies and scales our rendered frame to the buffer pointed to by data
void VideoSendThread::fillBuffer(void *data)
{
	videoDecoder_->setScaleDest(data, videoDecoder_->getWidth(),
								videoDecoder_->getHeight(), PIX_FMT_BGRA);
	videoDecoder_->scale(0);
}

void VideoSendThread::renderFrame()
{
    // we want our rendering code to be called by the shm_sink,
    // because it manages the shared memory synchronization
    sink_.render_callback(*this, bufferSize_);
}

bool VideoSendThread::captureFrame()
{
	int ret = videoDecoder_->decode();

	if (ret <= 0) {
		if (ret < 0)
			threadRunning_ = false;
		return false;
	}

	videoEncoder_->scale(videoDecoder_->getDecodedFrame(), 0);

    return true;
}

void VideoSendThread::encodeAndSendVideo()
{
	bool is_keyframe = forceKeyFrame_ > 0;

	if (is_keyframe)
		atomic_decrement(&forceKeyFrame_);

	EXIT_IF_FAIL(videoEncoder_->encode(is_keyframe, frameNumber_++) >= 0,
				 "encoding failed");
}

VideoSendThread::~VideoSendThread()
{
    set_false_atomic(&threadRunning_);
    Manager::instance().getVideoControls()->stoppedDecoding(id_, sink_.openedName());
    if (thread_)
        pthread_join(thread_, NULL);
}

void VideoSendThread::forceKeyFrame()
{
    atomic_increment(&forceKeyFrame_);
}

} // end namespace sfl_video
