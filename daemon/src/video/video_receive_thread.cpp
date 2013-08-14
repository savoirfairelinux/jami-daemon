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
#include "socket_pair.h"
#include "client/video_controls.h"
#include "check.h"
#include "video_decoder.h"

#include <unistd.h>
#include <map>

#include "manager.h"


namespace sfl_video {

	using std::string;
	const int SDP_BUFFER_SIZE = 8192;

// We do this setup here instead of the constructor because we don't want the
// main thread to block while this executes, so it happens in the video thread.
	void VideoReceiveThread::setupDecoder(VideoDecoder *decoder)
	{
		videoDecoder_ = decoder;

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

		if (!args_["framerate"].empty())
			decoder->setOption("framerate", args_["framerate"].c_str());
		if (!args_["video_size"].empty())
			decoder->setOption("video_size", args_["video_size"].c_str());
		if (!args_["channel"].empty())
			decoder->setOption("channel", args_["channel"].c_str());

		decoder->setInterruptCallback(interruptCb, this);

		if (input == SDP_FILENAME) {
#if HAVE_SDP_CUSTOM_IO
			// custom_io so the SDP demuxer will not open any UDP connections
			decoder->setOption("sdp_flags", "custom_io");
#else
			WARN("libavformat too old for custom SDP demuxing");
#endif

			EXIT_IF_FAIL(not stream_.str().empty(), "No SDP loaded");
			decoder->setIOContext(&sdpContext_);
		}

		if (decoder->openInput(input, format_str))
			EXIT_IF_FAIL(false, "Could not open input \"%s\"", input.c_str());

		if (input == SDP_FILENAME) {
#if HAVE_SDP_CUSTOM_IO
			// Now replace our custom AVIOContext with one that will read
			// packets
			decoder->setIOContext(demuxContext_);
#endif
		}

		// FIXME: this is a hack because our peer sends us RTP before
		// we're ready for it, and we miss the SPS/PPS. We should be
		// ready earlier.
		sleep(1);
		DEBUG("Finding stream info");
		if (requestKeyFrameCallback_)
			requestKeyFrameCallback_(id_);

		EXIT_IF_FAIL(!decoder->setupFromVideoData(),
					 "decoder IO startup failed");

		// Default size from input video
		if (dstWidth_ == 0 and dstHeight_ == 0) {
			dstWidth_ = decoder->getWidth();
			dstHeight_ = decoder->getHeight();
		}

		// determine required buffer size and allocate buffer
		bufferSize_ = VideoDecoder::getBufferSize(PIX_FMT_BGRA,
												  dstWidth_, dstHeight_);
		EXIT_IF_FAIL(bufferSize_ > 0, "Incorrect buffer size for decoding");

		EXIT_IF_FAIL(sink_.start(), "Cannot start shared memory sink");
		Manager::instance().getVideoControls()->startedDecoding(id_,
																sink_.openedName(),
																dstWidth_,
																dstHeight_);
		DEBUG("RX: shm sink started with size %d, width %d and height %d", bufferSize_,
			  dstWidth_, dstHeight_);
	}

// This callback is used by libav internally to break out of blocking calls
	int VideoReceiveThread::interruptCb(void *ctx)
	{
		VideoReceiveThread *context = static_cast<VideoReceiveThread*>(ctx);
		return not context->threadRunning_;
	}

	VideoReceiveThread::VideoReceiveThread(const std::string &id,
										   const std::map<string, string> &args) :
		args_(args),
		videoDecoder_(0),
		dstWidth_(0),
		dstHeight_(0),
		sink_(),
		threadRunning_(false),
		bufferSize_(0),
		id_(id),
		requestKeyFrameCallback_(0),
		stream_(args_["receiving_sdp"]),
		sdpContext_(SDP_BUFFER_SIZE, &readFunction, 0, 0, this),
		demuxContext_(0),
		thread_(0)
	{
	}

	int VideoReceiveThread::readFunction(void *opaque, uint8_t *buf,
										 int buf_size)
	{
		std::istream &is = static_cast<VideoReceiveThread*>(opaque)->stream_;
		is.read(reinterpret_cast<char*>(buf), buf_size);
		return is.gcount();
	}

	void VideoReceiveThread::addIOContext(SocketPair &socketPair)
	{
#if HAVE_SDP_CUSTOM_IO
		demuxContext_ = socketPair.getIOContext();
#else
		(void) socketPair;
#endif
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
	void VideoReceiveThread::fillBuffer(void *data)
	{
		videoDecoder_->setScaleDest(data, dstWidth_, dstHeight_, PIX_FMT_BGRA);
		videoDecoder_->scale(NULL, 0);
	}

	void VideoReceiveThread::run()
	{
		VideoDecoder *videoDecoder = new VideoDecoder();
		setupDecoder(videoDecoder);

		while (threadRunning_) {
			if (decodeFrame())
				renderFrame();
		}

		if (demuxContext_)
			delete demuxContext_;
		delete videoDecoder;
	}

	bool VideoReceiveThread::decodeFrame()
	{
		int ret = videoDecoder_->decode();

		// fatal error?
		if (ret == -1) {
			threadRunning_ = false;
			return false;
		}

		// decoding error?
		if (ret == -2 and requestKeyFrameCallback_) {
			videoDecoder_->setupFromVideoData();
			requestKeyFrameCallback_(id_);
		}

		return (ret<0?0:1);
	}

	void VideoReceiveThread::renderFrame()
	{
		// we want our rendering code to be called by the shm_sink,
		// because it manages the shared memory synchronization
		sink_.render_callback(*this, bufferSize_);
	}

	VideoReceiveThread::~VideoReceiveThread()
	{
		set_false_atomic(&threadRunning_);
		Manager::instance().getVideoControls()->stoppedDecoding(id_, sink_.openedName());
		// waits for the run() method (in separate thread) to return
		if (thread_)
			pthread_join(thread_, NULL);
	}

	void VideoReceiveThread::setRequestKeyFrameCallback(
		void (*cb)(const std::string &))
	{
		requestKeyFrameCallback_ = cb;
	}

	void VideoReceiveThread::addDetails(
		std::map<std::string, std::string> &details)
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
