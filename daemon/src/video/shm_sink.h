/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *
 *  Portions derived from GStreamer:
 *  Copyright (C) <2009> Collabora Ltd
 *  @author: Olivier Crete <olivier.crete@collabora.co.uk
 *  Copyright (C) <2009> Nokia Inc
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

#ifndef SHM_SINK_H_
#define SHM_SINK_H_

#include "noncopyable.h"
#include "video_provider.h"
#include "video_base.h"

#include <string>
#include <vector>

class SHMHeader;

namespace sfl_video {

class SHMSink : public VideoFramePassiveReader
{
public:
    SHMSink(const std::string &shm_name = "");
    std::string openedName() const { return opened_name_; }
    ~SHMSink();

    bool start();
    bool stop();

    bool resize_area(size_t desired_length);

    void render(const std::vector<unsigned char> &data);
    void render_frame(VideoFrame& src);
    void render_callback(VideoProvider &provider, size_t bytes);

    // as VideoFramePassiveReader
    void update(Observable<std::shared_ptr<VideoFrame> >*, std::shared_ptr<VideoFrame> &);

private:
    NON_COPYABLE(SHMSink);

    void shm_lock();
    void shm_unlock();
    std::string shm_name_;
    int fd_;
    SHMHeader *shm_area_;
    size_t shm_area_len_;
    std::string opened_name_;
#ifdef DEBUG_FPS
    unsigned frameCount_;
    std::chrono::time_point<std::chrono::system_clock> lastFrameDebug_;
#endif
};

}

#endif // SHM_SINK_H_
