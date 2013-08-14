/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
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

#include "video_decoder.h"
#include "check.h"

#include <iostream>

// libav includes
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 28, 0)
// fallback to av_freep for older libavcodec
#define avcodec_free_frame av_freep
#endif

namespace sfl_video {

	using std::string;

	VideoDecoder::VideoDecoder() :
		inputDecoder_(0),
		decoderCtx_(0),
		rawFrames_(),
		lockedFrame_(-1),
		lockedFrameCnt_(0),
		lastFrame_(1),
		inputCtx_(avformat_alloc_context()),
		imgConvertCtx_(0),
		interruptCb_(),
		scalerCtx_(0),
		scaledPicture_(),
		accessMutex_(),
		streamIndex_(-1),
		dstWidth_(0),
		dstHeight_(0)
	{
		pthread_mutex_init(&accessMutex_, NULL);
	}

	VideoDecoder::~VideoDecoder()
	{
		if (decoderCtx_)
			avcodec_close(decoderCtx_);

		if (inputCtx_ and inputCtx_->nb_streams > 0) {
#if LIBAVFORMAT_VERSION_MAJOR < 54
			av_close_input_file(inputCtx_);
#else
			avformat_close_input(&inputCtx_);
#endif
		}

		sws_freeContext(scalerCtx_);

		pthread_mutex_destroy(&accessMutex_);
	}

	int VideoDecoder::openInput(const std::string &source_str,
								const std::string &format_str)
	{
		AVInputFormat *iformat = av_find_input_format(format_str.c_str());

		if (!iformat) {
			ERROR("Cannot find format \"%s\"", format_str.c_str());
			return -1;
		}

		int ret = avformat_open_input(&inputCtx_, source_str.c_str(), iformat,
									  NULL);
		if (ret)
			ERROR("avformat_open_input failed (%d)", ret);
		return ret;
	}

	void VideoDecoder::setInterruptCallback(int (*cb)(void*), void *opaque)
	{
		if (cb) {
			interruptCb_.callback = cb;
			interruptCb_.opaque = opaque;
			inputCtx_->interrupt_callback = interruptCb_;
		} else {
			inputCtx_->interrupt_callback.callback = 0;
		}
	}

	void VideoDecoder::setIOContext(VideoIOHandle *ioctx)
	{
		inputCtx_->pb = ioctx->get();
	}

	int VideoDecoder::setupFromVideoData()
	{
		int ret;

		if (decoderCtx_)
			avcodec_close(decoderCtx_);

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
		ret = av_find_stream_info(inputCtx_);
#else
		ret = avformat_find_stream_info(inputCtx_, options_ ? &options_ : NULL);
#endif

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
			ERROR("Could not find stream info: %s", errBuf);
			return -1;
		}

		// find the first video stream from the input
		for (size_t i = 0; streamIndex_ == -1 && i < inputCtx_->nb_streams; ++i)
			if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
				streamIndex_ = i;

		if (streamIndex_ == -1) {
			ERROR("Could not find video stream");
			return -1;
		}

		// Get a pointer to the codec context for the video stream
		decoderCtx_ = inputCtx_->streams[streamIndex_]->codec;
		if (decoderCtx_ == 0) {
			ERROR("Decoder context is NULL");
			return -1;
		}

		// find the decoder for the video stream
		inputDecoder_ = avcodec_find_decoder(decoderCtx_->codec_id);
		if (!inputDecoder_) {
			ERROR("Unsupported codec");
			return -1;
		}

		decoderCtx_->thread_count = 1;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
		ret = avcodec_open(decoderCtx_, inputDecoder_);
#else
		ret = avcodec_open2(decoderCtx_, inputDecoder_, NULL);
#endif
		if (ret) {
			ERROR("Could not open codec");
			return -1;
		}

		dstWidth_ = decoderCtx_->width;
		dstHeight_ = decoderCtx_->height;

		return 0;
	}

	int VideoDecoder::decode()
	{
		int ret = 0;

		// Guarantee that we free the packet every iteration
		VideoPacket video_packet;
		AVPacket *inpacket = video_packet.get();
		ret = av_read_frame(inputCtx_, inpacket);
		if (ret == AVERROR(EAGAIN))
			return 0;
		else if (ret < 0) {
			ERROR("Couldn't read frame: %s\n", strerror(ret));
			return -1;
		}

		int idx;
		pthread_mutex_lock(&accessMutex_);
		if (lockedFrame_ >= 0)
			idx = !lockedFrame_;
		else
			idx = !lastFrame_;
		pthread_mutex_unlock(&accessMutex_);

		AVFrame *frame = rawFrames_[idx].get();
		avcodec_get_frame_defaults(frame);

		// is this a packet from the video stream?
		if (inpacket->stream_index != streamIndex_)
			return 0;

		int frameFinished = 0;
		const int len = avcodec_decode_video2(decoderCtx_, frame,
											  &frameFinished, inpacket);
		if (len <= 0)
			return -2;

		if (frameFinished) {
			pthread_mutex_lock(&accessMutex_);
			lastFrame_ = idx;
			pthread_mutex_unlock(&accessMutex_);
		}

		return frameFinished;
	}

	int VideoDecoder::flush()
	{
		AVPacket inpacket;

		av_init_packet(&inpacket);
		inpacket.data = NULL;
		inpacket.size = 0;

		int idx;
		pthread_mutex_lock(&accessMutex_);
		if (lockedFrame_ >= 0)
			idx = !lockedFrame_;
		else
			idx = !lastFrame_;
		pthread_mutex_unlock(&accessMutex_);

		AVFrame *frame = rawFrames_[idx].get();
		avcodec_get_frame_defaults(frame);

		int frameFinished = 0;
		const int len = avcodec_decode_video2(decoderCtx_, frame,
											  &frameFinished, &inpacket);
		if (len <= 0)
		  return -2;

		if (frameFinished) {
			pthread_mutex_lock(&accessMutex_);
			lastFrame_ = idx;
			pthread_mutex_unlock(&accessMutex_);
		}

		return frameFinished;
	}

	void VideoDecoder::setScaleDest(void *data, int width, int height,
									int pix_fmt)
	{
		AVFrame *output_frame = scaledPicture_.get();
		avpicture_fill((AVPicture *) output_frame, (uint8_t *) data,
					   (PixelFormat) pix_fmt, width, height);
		output_frame->format = pix_fmt;
		output_frame->width = width;
		output_frame->height = height;
	}

	void* VideoDecoder::scale(SwsContext *ctx, int flags)
	{
		AVFrame *output_frame = scaledPicture_.get();

		ctx = sws_getCachedContext(ctx,
								   decoderCtx_->width,
								   decoderCtx_->height,
								   decoderCtx_->pix_fmt,
								   output_frame->width,
								   output_frame->height,
								   (PixelFormat) output_frame->format,
								   SWS_BICUBIC, /* FIXME: option? */
								   NULL, NULL, NULL);
		if (ctx) {
			VideoFrame *frame = lockFrame();
			if (frame) {
				AVFrame *frame_ = frame->get();
				sws_scale(ctx, frame_->data, frame_->linesize, 0,
						  decoderCtx_->height, output_frame->data,
						  output_frame->linesize);
				unlockFrame();
			}
		} else {
			ERROR("Unable to create a scaler context");
		}

		return ctx;
	}

	VideoFrame *VideoDecoder::lockFrame()
	{
		VideoFrame *frame;

		pthread_mutex_lock(&accessMutex_);
		if (lockedFrame_ >= 0) {
			lockedFrameCnt_++;
		} else {
			lockedFrame_ = lastFrame_;
			lockedFrameCnt_ = 0;
		}
		pthread_mutex_unlock(&accessMutex_);

		if (lockedFrame_ >= 0)
			return &rawFrames_[lockedFrame_];
		return NULL;
	}

	void VideoDecoder::unlockFrame()
	{
		pthread_mutex_lock(&accessMutex_);
		if (lockedFrameCnt_ > 0)
			lockedFrameCnt_--;
		else
			lockedFrame_ = -1;
		pthread_mutex_unlock(&accessMutex_);
	}
}
