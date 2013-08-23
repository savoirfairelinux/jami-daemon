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

#ifndef _VIDEO_DECODER_H_
#define _VIDEO_DECODER_H_

#include "video_base.h"
#include "video_scaler.h"
#include "noncopyable.h"

#include <pthread.h>
#include <string>

class AVCodecContext;
class AVStream;
class AVFormatContext;
class AVCodec;

namespace sfl_video {

	class VideoDecoder : public VideoCodec {
	public:
		VideoDecoder();
		~VideoDecoder();

		void setInterruptCallback(int (*cb)(void*), void *opaque);
		void setIOContext(VideoIOHandle *ioctx);
		int openInput(const std::string &source_str,
					  const std::string &format_str);
		int setupFromVideoData();
		int decode();
		int flush();
		void scale(VideoScaler &ctx, VideoFrame &output);
		VideoFrame *lockFrame();
		void unlockFrame();

		int getWidth() const { return dstWidth_; }
		int getHeight() const { return dstHeight_; }

	private:
		NON_COPYABLE(VideoDecoder);

		AVCodec *inputDecoder_;
		AVCodecContext *decoderCtx_;
		VideoFrame rawFrames_[2];
		int lockedFrame_;
		int lockedFrameCnt_;
		int lastFrame_;
		AVFormatContext *inputCtx_;
		VideoFrame scaledPicture_;
		pthread_mutex_t accessMutex_;

		int streamIndex_;
		int dstWidth_;
        int dstHeight_;
	};
}

#endif // _VIDEO_DECODER_H_
