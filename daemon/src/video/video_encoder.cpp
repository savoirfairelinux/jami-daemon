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

#include "video_encoder.h"
#include "check.h"

#include <iostream>
#include <sstream>

// libav includes
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 28, 0)
// fallback to av_freep for older libavcodec
#define avcodec_free_frame av_freep
#endif


namespace sfl_video {

	using std::string;

	VideoEncoder::VideoEncoder() :
		outputEncoder_(0),
		encoderCtx_(0),
		outputCtx_(avformat_alloc_context()),
		imgConvertCtx_(0),
		interruptCb_(),
		scalerCtx_(0),
		scaledPicture_(avcodec_alloc_frame()),
		stream_(0),
		encoderBuffer_(0),
		encoderBufferSize_(0),
		streamIndex_(-1),
		dstWidth_(0),
		dstHeight_(0)
	{ }

	VideoEncoder::~VideoEncoder()
	{
		if (outputCtx_ and outputCtx_->priv_data)
            av_write_trailer(outputCtx_);

		if (encoderCtx_)
			avcodec_close(encoderCtx_);

		sws_freeContext(scalerCtx_);
		avcodec_free_frame(&scaledPicture_);
		av_free(encoderBuffer_);
	}

	int VideoEncoder::openOutput(const char *enc_name, const char *short_name,
								 const char *filename, const char *mime_type)
	{
		AVOutputFormat *oformat = av_guess_format(short_name, filename,
												  mime_type);

		if (!oformat) {
			ERROR("Unable to find a suitable output format for %s", filename);
			return -1;
		}

		outputCtx_->oformat = oformat;
		strncpy(outputCtx_->filename, filename, sizeof(outputCtx_->filename));
		// guarantee that buffer is NULL terminated
		outputCtx_->filename[sizeof(outputCtx_->filename) - 1] = '\0';

		/* find the video encoder */
		outputEncoder_ = avcodec_find_encoder_by_name(enc_name);
		if (!outputEncoder_) {
			ERROR("Encoder \"%s\" not found!", enc_name);
			return -1;
		}

		prepareEncoderContext();

		/* let x264 preset override our encoder settings */
		if (!strcmp(enc_name, "libx264")) {
			AVDictionaryEntry *entry = av_dict_get(options_, "parameters",
												  NULL, 0);
			// FIXME: this should be parsed from the fmtp:profile-level-id
			// attribute of our peer, it will determine what profile and
			// level we are sending (i.e. that they can accept).
			extractProfileLevelID(entry?entry->value:"", encoderCtx_);
			forcePresetX264();
		}

		int ret;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
		ret = avcodec_open(encoderCtx_, outputEncoder_);
#else
		ret = avcodec_open2(encoderCtx_, outputEncoder_, NULL);
#endif
		if (ret) {
			ERROR("Could not open encoder");
			return -1;
		}

		// add video stream to outputformat context
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
		stream_ = av_new_stream(outputCtx_, 0);
#else
		stream_ = avformat_new_stream(outputCtx_, 0);
#endif
		if (!stream_) {
			ERROR("Could not allocate stream");
			return -1;
		}
		stream_->codec = encoderCtx_;

		// allocate buffers for both scaled (pre-encoder) and encoded frames
		encoderBufferSize_ = getBufferSize(encoderCtx_->pix_fmt,
										   encoderCtx_->width,
										   encoderCtx_->height);
		if (encoderBufferSize_ <= FF_MIN_BUFFER_SIZE) {
			ERROR("Encoder buffer too small");
			return -1;
		}

		encoderBuffer_ = (uint8_t*) av_malloc(encoderBufferSize_);

		setScaleDest(encoderBuffer_,
					encoderCtx_->width,
					encoderCtx_->height,
					encoderCtx_->pix_fmt);

		return 0;
	}

	void VideoEncoder::setInterruptCallback(int (*cb)(void*), void *opaque)
	{
		if (cb) {
			interruptCb_.callback = cb;
			interruptCb_.opaque = opaque;
			outputCtx_->interrupt_callback = interruptCb_;
		} else {
			outputCtx_->interrupt_callback.callback = 0;
		}
	}

	void VideoEncoder::setIOContext(VideoIOHandle *ioctx)
	{
		outputCtx_->pb = ioctx->get();
		outputCtx_->packet_size = outputCtx_->pb->buffer_size;
	}

	int VideoEncoder::startIO()
	{
		int ret = avformat_write_header(outputCtx_,
										options_ ? &options_ : NULL);
		if (ret) {
			ERROR("Could not write header for output file..."
				  "check codec parameters");
			return -1;
		}

		av_dump_format(outputCtx_, 0, outputCtx_->filename, 1);

		return 0;
	}

	int VideoEncoder::encode(bool is_keyframe, int frame_number)
	{
		int ret;

		if (frame_number > 0)
			scaledPicture_->pts = frame_number;

		if (is_keyframe) {
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(53, 20, 0)
			scaledPicture_->pict_type = AV_PICTURE_TYPE_I;
#else
			scaledPicture_->pict_type = FF_I_TYPE;
#endif
		} else {
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(53, 20, 0)
			/* FIXME: Should be AV_PICTURE_TYPE_NONE for newer libavutil */
			scaledPicture_->pict_type = (AVPictureType) 0;
#else
			scaledPicture_->pict_type = (AVPictureType) 0;
#endif
		}

		AVPacket opkt;
		av_init_packet(&opkt);

		opkt.data = encoderBuffer_;

#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(54, 0, 0)
		int got_packet = 0;
		opkt.size = encoderBufferSize_;
		ret = avcodec_encode_video2(encoderCtx_, &opkt, scaledPicture_,
									&got_packet);
		if (ret < 0) {
			ERROR("avcodec_encode_video failed");
			return -1;
		}

		if (!got_packet)
			return 0;

		opkt.pts = frame_number;
#else
	    ret = avcodec_encode_video(encoderCtx_, encoderBuffer_,
								   encoderBufferSize_, scaledPicture_);
		if (ret <= 0) {
			ERROR("avcodec_encode_video failed");
			return -1;
		}
		opkt.size = ret;

		// rescale pts from encoded video framerate to rtp clock rate
		if (encoderCtx_->coded_frame->pts != (int64_t) AV_NOPTS_VALUE)
			opkt.pts = av_rescale_q(encoderCtx_->coded_frame->pts,
									encoderCtx_->time_base, stream_->time_base);
		else
			opkt.pts = 0;
#endif

		// is it a key frame?
		if (encoderCtx_->coded_frame->key_frame)
			opkt.flags |= AV_PKT_FLAG_KEY;
		opkt.stream_index = stream_->index;

		// write the compressed frame
		ret = av_interleaved_write_frame(outputCtx_, &opkt);
		if (ret)
			ERROR("interleaved_write_frame failed");

		return ret;
	}

	int VideoEncoder::flush()
	{
		int ret;
		AVPacket opkt;
		av_init_packet(&opkt);

		opkt.data = encoderBuffer_;

#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(54, 0, 0)
		int got_packet = 0;
		opkt.size = encoderBufferSize_;
		ret = avcodec_encode_video2(encoderCtx_, &opkt, NULL, &got_packet);
		if (ret < 0) {
			ERROR("avcodec_encode_video failed");
			return -1;
		}

		if (!got_packet)
			return 0;
#else
		ret = avcodec_encode_video(encoderCtx_, encoderBuffer_,
								   encoderBufferSize_, NULL);
		if (ret <= 0) {
			ERROR("avcodec_encode_video failed");
			return -1;
		}
		opkt.size = ret;
#endif

		// write the compressed frame
		ret = av_interleaved_write_frame(outputCtx_, &opkt);
		if (ret)
			ERROR("interleaved_write_frame failed");

		return ret;
	}

	void VideoEncoder::setScaleDest(void *data, int width, int height,
								   int pix_fmt)
	{
		avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
					   static_cast<uint8_t *>(data),
					   (PixelFormat)(pix_fmt), width, height);
		scaledPicture_->format = pix_fmt;
		scaledPicture_->width = width;
		scaledPicture_->height = height;
	}

	void VideoEncoder::scale(VideoFrame *frame_, int flags)
	{
		AVFrame *src_frame = frame_->get();

		scalerCtx_ = sws_getCachedContext(scalerCtx_,
										  src_frame->width,
										  src_frame->height,
										  (PixelFormat)(scaledPicture_->format),
										  encoderCtx_->width,
										  encoderCtx_->height,
										  encoderCtx_->pix_fmt,
										  SWS_BICUBIC, /* FIXME: option? */
										  NULL, NULL, NULL);

		if (!scalerCtx_) {
			ERROR("Unable to create a scaler context");
			return;
		}

		sws_scale(scalerCtx_, src_frame->data, src_frame->linesize,
				  0, src_frame->height,
				  scaledPicture_->data, scaledPicture_->linesize);
	}

	void VideoEncoder::print_sdp(std::string &sdp_)
	{
		/* theora sdp can be huge */
		const size_t sdp_size = outputCtx_->streams[0]->codec->extradata_size \
			+ 2048;
		std::string sdp(sdp_size, 0);
		av_sdp_create(&outputCtx_, 1, &(*sdp.begin()), sdp_size);
		std::istringstream iss(sdp);
		string line;
		sdp_ = "";
		while (std::getline(iss, line)) {
			/* strip windows line ending */
			line = line.substr(0, line.length() - 1);
			sdp_ += line + "\n";
		}
		DEBUG("sending\n%s", sdp_.c_str());
	}

	void VideoEncoder::prepareEncoderContext()
	{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 12, 0)
		encoderCtx_ = avcodec_alloc_context();
		avcodec_get_context_defaults(encoderCtx_);
		(void) outputEncoder_;
#else
		encoderCtx_ = avcodec_alloc_context3(outputEncoder_);
#endif

		// set some encoder settings here
		encoderCtx_->bit_rate = 1000 * atoi(av_dict_get(options_, "bitrate",
														NULL, 0)->value);
		DEBUG("Using bitrate %d", encoderCtx_->bit_rate);

		// resolution must be a multiple of two
		char *width = av_dict_get(options_, "width", NULL, 0)->value;
		dstWidth_ = encoderCtx_->width = width ? atoi(width) : 0;
		char *height = av_dict_get(options_, "height", NULL, 0)->value;
		dstHeight_ = encoderCtx_->height = height ? atoi(height) : 0;

		const char *framerate = av_dict_get(options_, "framerate",
											NULL, 0)->value;
		const int DEFAULT_FPS = 30;
		const int fps = framerate ? atoi(framerate) : DEFAULT_FPS;
		encoderCtx_->time_base = (AVRational) {1, fps};
		// emit one intra frame every gop_size frames
		encoderCtx_->max_b_frames = 0;
		encoderCtx_->pix_fmt = PIX_FMT_YUV420P; // TODO: option me !

		// Fri Jul 22 11:37:59 EDT 2011:tmatth:XXX: DON'T set this, we want our
		// pps and sps to be sent in-band for RTP
		// This is to place global headers in extradata instead of every
		// keyframe.
		// encoderCtx_->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	void VideoEncoder::forcePresetX264()
	{
		const char *speedPreset = "ultrafast";
		if (av_opt_set(encoderCtx_->priv_data, "preset", speedPreset, 0))
			WARN("Failed to set x264 preset '%s'", speedPreset);
		const char *tune = "zerolatency";
		if (av_opt_set(encoderCtx_->priv_data, "tune", tune, 0))
			WARN("Failed to set x264 tune '%s'", tune);
	}

	void VideoEncoder::extractProfileLevelID(const std::string &parameters,
											 AVCodecContext *ctx)
	{
		// From RFC3984:
		// If no profile-level-id is present, the Baseline Profile without
		// additional constraints at Level 1 MUST be implied.
		ctx->profile = FF_PROFILE_H264_BASELINE;
		ctx->level = 0x0d;
		// ctx->level = 0x0d; // => 13 aka 1.3
		if (parameters.empty())
			return;

		const std::string target("profile-level-id=");
		size_t needle = parameters.find(target);
		if (needle == std::string::npos)
			return;

		needle += target.length();
		const size_t id_length = 6; /* digits */
		const std::string profileLevelID(parameters.substr(needle, id_length));
		if (profileLevelID.length() != id_length)
			return;

		int result;
		std::stringstream ss;
		ss << profileLevelID;
		ss >> std::hex >> result;
		// profile-level id consists of three bytes
		const unsigned char profile_idc = result >> 16;             // 42xxxx -> 42
		const unsigned char profile_iop = ((result >> 8) & 0xff);   // xx80xx -> 80
		ctx->level = result & 0xff;                                 // xxxx0d -> 0d
		switch (profile_idc) {
		case FF_PROFILE_H264_BASELINE:
			// check constraint_set_1_flag
			if ((profile_iop & 0x40) >> 6)
				ctx->profile |= FF_PROFILE_H264_CONSTRAINED;
			break;
		case FF_PROFILE_H264_HIGH_10:
		case FF_PROFILE_H264_HIGH_422:
		case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
			// check constraint_set_3_flag
			if ((profile_iop & 0x10) >> 4)
				ctx->profile |= FF_PROFILE_H264_INTRA;
			break;
		}
		DEBUG("Using profile %x and level %d", ctx->profile, ctx->level);
	}
}
