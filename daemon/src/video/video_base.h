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

#ifndef _VIDEO_BASE_H_
#define _VIDEO_BASE_H_

#include "noncopyable.h"
#include "logger.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

/* LIBAVFORMAT_VERSION_CHECK checks for the right version of libav and FFmpeg
 * a is the major version
 * b and c the minor and micro versions of libav
 * d and e the minor and micro versions of FFmpeg */
#define LIBAVFORMAT_VERSION_CHECK( a, b, c, d, e )                      \
    ( (LIBAVFORMAT_VERSION_MICRO <  100 && LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT( a, b, c ) ) || \
      (LIBAVFORMAT_VERSION_MICRO >= 100 && LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT( a, d, e ) ) )

#define HAVE_SDP_CUSTOM_IO LIBAVFORMAT_VERSION_CHECK(54,20,3,59,103)

typedef int(*io_readcallback)(void *opaque, uint8_t *buf, int buf_size);
typedef int(*io_writecallback)(void *opaque, uint8_t *buf, int buf_size);
typedef int64_t(*io_seekcallback)(void *opaque, int64_t offset, int whence);

class AVPacket;
class AVDictionary;

namespace sfl_video {
	class VideoPacket {
	public:
        ~VideoPacket() { av_free_packet(&inpacket_); };

		AVPacket* get() const { return (AVPacket*)(&inpacket_); }

	private:
		AVPacket inpacket_;
	};

	class VideoIOHandle {
	public:
	VideoIOHandle(ssize_t buffer_size,
				  io_readcallback read_cb,
				  io_writecallback write_cb,
				  io_seekcallback seek_cb,
				  void *opaque, int writable) : ctx_(0), buf_(0)

		{
			buf_ = static_cast<unsigned char *>(av_malloc(buffer_size));
			ctx_ = avio_alloc_context(buf_, buffer_size, writable, opaque,
                                      read_cb, write_cb, seek_cb);
			ctx_->max_packet_size = buffer_size;
		}

		~VideoIOHandle() { av_free(ctx_); av_free(buf_); }

		AVIOContext *get() { return ctx_; }

	private:
		NON_COPYABLE(VideoIOHandle);

		AVIOContext *ctx_;
		unsigned char *buf_;
	};

	class VideoFrame {
	public:
	VideoFrame() : frame_(avcodec_alloc_frame()) {}

		~VideoFrame() { avcodec_free_frame(&frame_); };

		AVFrame *get() { return frame_; }

	private:
		NON_COPYABLE(VideoFrame);

		AVFrame *frame_;
	};

	class VideoCodec {
	public:
	VideoCodec() : options_(0) {};
		virtual ~VideoCodec() {};

		static size_t getBufferSize(PixelFormat pix_fmt, int width, int height)
		{
			return avpicture_get_size(pix_fmt, width, height);
		}

		void setOption(const char *name, const char *value)
		{
			av_dict_set(&options_, name, value, 0);
		}

	private:
		NON_COPYABLE(VideoCodec);

	protected:
		AVDictionary *options_;
	};
}

#endif // _VIDEO_BASE_H_
