/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
#include "manager.h"

#include <map>


namespace sfl_video {

using std::string;

VideoSendThread::VideoSendThread(const std::map<string, string> &args) :
    args_(args),
	videoPreview_(0),
	videoEncoder_(0),
    sdp_(),
	outputWidth_(0),
	outputHeight_(0),
    threadRunning_(false),
    forceKeyFrame_(0),
    thread_(0),
	frameNumber_(0),
    muxContext_(0)
{}

VideoSendThread::~VideoSendThread()
{
    set_false_atomic(&threadRunning_);
    if (thread_)
        pthread_join(thread_, NULL);
}

void VideoSendThread::setup()
{
    const char *enc_name = args_["codec"].c_str();
	int width, height;

	videoPreview_ = Manager::instance().getVideoControls()->getVideoPreview();
	EXIT_IF_FAIL(videoPreview_, "No previewing!");

	width = videoPreview_->getWidth();
	height = videoPreview_->getHeight();

	/* Encoder setup */
	if (!args_["width"].empty()) {
		const char *s = args_["width"].c_str();
		videoEncoder_->setOption("width", s);
		width = outputWidth_ = atoi(s);
	} else {
		char buf[11];
		outputWidth_ = width;
		sprintf(buf, "%10d", width);
		videoEncoder_->setOption("width", buf);
	}

	if (!args_["height"].empty()) {
		const char *s = args_["height"].c_str();
		videoEncoder_->setOption("height", s);
		height = outputHeight_ = atoi(s);
	} else {
		char buf[11];
		outputHeight_ = height;
		sprintf(buf, "%10d", height);
		videoEncoder_->setOption("height", buf);
	}

	videoEncoder_->setInterruptCallback(interruptCb, this);

	videoEncoder_->setOption("bitrate", args_["bitrate"].c_str());

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
				 "encoder openOutput() failed");
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
	videoEncoder_ = new VideoEncoder();

	setup();

    while (threadRunning_) {
		encodeAndSendVideo();
	}

	delete videoEncoder_;

	if (muxContext_)
		delete muxContext_;
}

void VideoSendThread::encodeAndSendVideo()
{
	bool is_keyframe = forceKeyFrame_ > 0;

	if (is_keyframe)
		atomic_decrement(&forceKeyFrame_);

	VideoFrame *inputframe = videoPreview_->lockFrame();
	if (inputframe) {
        EXIT_IF_FAIL(videoEncoder_->encode(*inputframe, is_keyframe, frameNumber_++) >= 0,
                     "encoding failed");

    }
}

void VideoSendThread::forceKeyFrame()
{
    atomic_increment(&forceKeyFrame_);
}

} // end namespace sfl_video
