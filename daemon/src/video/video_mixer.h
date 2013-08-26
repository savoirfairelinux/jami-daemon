/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *
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

#ifndef __VIDEO_MIXER_H__
#define __VIDEO_MIXER_H__

#include "noncopyable.h"
#include "video_base.h"

#include <pthread.h>
#include <forward_list>

namespace sfl_video {
    using std::forward_list;

	class VideoMixer : public VideoSource
	{
	public:
		VideoMixer();
		~VideoMixer();

        void addVideoSource(VideoSource &source);
        void removeVideoSource(VideoSource &source);

        std::shared_ptr<VideoFrame> waitNewFrame();
        std::shared_ptr<VideoFrame> obtainLastFrame();
        int getWidth() const;
        int getHeight() const;

	private:
		NON_COPYABLE(VideoMixer);

		static int interruptCb(void *ctx);
		static void *runCallback(void *);
		void run();
		void setup();

        void waitForUpdate();
        void updated();
        void render();
        void encode();

        pthread_t thread_;
        bool threadRunning_;

        pthread_mutex_t updateMutex_;
        pthread_cond_t updateCondition_;
        bool updated_;

        forward_list<VideoSource*> sourceList_;
        int width;
        int height;
	};
}

#endif // __VIDEO_MIXER_H__
