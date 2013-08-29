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

#ifndef __VIDEO_SEND_THREAD_H__
#define __VIDEO_SEND_THREAD_H__

#include <map>
#include <string>
#include <tr1/memory>
#include "noncopyable.h"
#include "video_encoder.h"
#include "video_preview.h"
#include "video_mixer.h"


namespace sfl_video {

class SocketPair;

class VideoSendThread {
private:
    NON_COPYABLE(VideoSendThread);
    void setup();
    static int interruptCb(void *ctx);
    static void *runCallback(void *);
    void run();
    void encodeAndSendVideo(VideoFrame *);

    std::map<std::string, std::string> args_;
    /*-------------------------------------------------------------*/
    /* These variables should be used in thread (i.e. run()) only! */
    /*-------------------------------------------------------------*/
    VideoEncoder *videoEncoder_;
    VideoSource *videoSource_;

    std::string sdp_;
    int outputWidth_;
    int outputHeight_;

    bool threadRunning_;
    int forceKeyFrame_;
    pthread_t thread_;
    int frameNumber_;
    bool fromMixer_;
    VideoIOHandle* muxContext_;

public:
    VideoSendThread(const std::map<std::string, std::string> &args);
    ~VideoSendThread();
    void addIOContext(SocketPair &sock);
    void start();
    std::string getSDP() const { return sdp_; }
    void forceKeyFrame();
};
}

#endif // __VIDEO_SEND_THREAD_H__
