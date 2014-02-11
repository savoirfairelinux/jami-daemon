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
#include "video_scaler.h"
#include "shm_sink.h"
#include "sflthread.h"

#include <mutex>
#include <list>

namespace sfl_video {

/* VideoMixer is implemented in a Push/Push model */
class VideoMixer :
        public VideoFramePassiveReader, /* left side */
        public VideoGenerator           /* right side */
{
public:
    VideoMixer(const std::string &id);
    ~VideoMixer();

    void setDimensions(int width, int height);

    int getWidth() const;
    int getHeight() const;
    int getPixelFormat() const;

    // as VideoFramePassiveReader
    void attached(VideoFrameActiveWriter&);
    void detached(VideoFrameActiveWriter&);
    void update(VideoFrameActiveWriter&, VideoFrameShrPtr&);

private:
    NON_COPYABLE(VideoMixer);

    void render_frame(VideoFrame& input, const int index);
    void start_sink();
    void stop_sink();

    const std::string id_;
    int width_;
    int height_;
    std::list<VideoFrameActiveWriter *> sources_;
    std::mutex mutex_;
    SHMSink sink_;
    static const char * VIDEO_MIXER_SUFFIX;
};

}

#endif // __VIDEO_MIXER_H__
