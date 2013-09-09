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
#include "video_scaler.h"
#include "sflthread.h"

#include <pthread.h>
#include <list>

namespace sfl_video {
using std::forward_list;

class VideoMixer : public VideoGenerator, public SFLThread
{
public:
    VideoMixer();
    ~VideoMixer();

    void setDimensions(int width, int height);
    void addSource(VideoSource *source);
    void removeSource(VideoSource *source);
    void clearSources();
    void render();

    int getWidth() const;
    int getHeight() const;

    // threading
    void process();

private:
    NON_COPYABLE(VideoMixer);

    void waitForUpdate();
    void encode();
    void rendering();

    pthread_mutex_t updateMutex_;
    pthread_cond_t updateCondition_;
    VideoScaler sourceScaler_;
    VideoFrame scaledFrame_;

    std::list<VideoSource*> sourceList_;
    int width_;
    int height_;
};

}

#endif // __VIDEO_MIXER_H__
