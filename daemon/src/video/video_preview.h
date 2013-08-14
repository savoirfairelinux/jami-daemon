/*
 *  Copyright (C) 2011-2012 Savoir-Faire Linux Inc.
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

#ifndef __VIDEO_PREVIEW_H__
#define __VIDEO_PREVIEW_H__

#include "noncopyable.h"
#include "shm_sink.h"
#include "video_provider.h"

#include <tr1/memory>
#include <pthread.h>
#include <string>
#include <map>

namespace sfl_video {
	using std::string;

	class VideoDecoder;
	class VideoFrame;

	class VideoPreview : public VideoProvider
	{
	public:
		VideoPreview(const std::map<string, string> &args);
		~VideoPreview();
		int getWidth() const;
		int getHeight() const;
		void fill(void *data, int width, int height);
		VideoFrame *lockFrame();
		void unlockFrame();

	private:
		NON_COPYABLE(VideoPreview);

		std::string id_;
		std::map<string, string> args_;
		VideoDecoder *decoder_;
		bool threadRunning_;
		pthread_t thread_;
		pthread_mutex_t accessMutex_;
		SHMSink sink_;
		size_t bufferSize_;
		int previewWidth_;
		int previewHeight_;

		static int interruptCb(void *ctx);
		static void *runCallback(void *);
		void fillBuffer(void *data);
		void run();
		bool captureFrame();
		void setup();
		void renderFrame();
	};
}

#endif // __VIDEO_PREVIEW_H__
