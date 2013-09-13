/*
 *  Copyright (C) 2011-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifndef __VIDEO_SEND_THREAD_H__
#define __VIDEO_SEND_THREAD_H__

#include "noncopyable.h"
#include "video_encoder.h"
#include "video_mixer.h"

#include <map>
#include <string>
#include <memory>

namespace sfl_video {

class SocketPair;

class VideoSendThread : public VideoFramePassiveReader
{
public:
    VideoSendThread(const std::string &id,
                    const std::map<std::string, std::string> &args,
                    SocketPair& socketPair,
                    VideoFrameActiveWriter *local_video,
                    VideoFrameActiveWriter *mixer);
    virtual ~VideoSendThread();
    std::string getSDP() const { return sdp_; }
    void forceKeyFrame();

    // as VideoFramePassiveReader
    void update(Observable<VideoFrameSP>*, VideoFrameSP&);

private:
    NON_COPYABLE(VideoSendThread);

    bool setup();
    void encodeAndSendVideo(VideoFrame&);

    std::map<std::string, std::string> args_;
    const std::string &id_;

    VideoEncoder *videoEncoder_;
    VideoFrameActiveWriter *videoSource_;

    int forceKeyFrame_;
    int frameNumber_;
    VideoIOHandle* muxContext_;
    std::string sdp_;
};

}

#endif // __VIDEO_SEND_THREAD_H__
