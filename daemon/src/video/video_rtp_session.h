/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef __VIDEO_RTP_SESSION_H__
#define __VIDEO_RTP_SESSION_H__

#include "video_base.h"
#include "video_mixer.h"
#include "noncopyable.h"
#include "video_sender.h"
#include "video_receive_thread.h"
#include "socket_pair.h"

#include <string>
#include <map>
#include <memory>
#include <mutex>

class Sdp;
class Conference;

namespace sfl_video {

class VideoRtpSession {
public:
    VideoRtpSession(const std::string &callID,
                    const std::map<std::string, std::string> &txArgs);
    ~VideoRtpSession();

    void start(int localPort);
    void stop();
    void updateDestination(const std::string &destination,
                           unsigned int port);
    void updateSDP(const Sdp &sdp);
    void forceKeyFrame();
    void bindMixer(VideoMixer* mixer);
    void unbindMixer();
    void enterConference(Conference* conference);
    void exitConference();

private:
    NON_COPYABLE(VideoRtpSession);

    void setupConferenceVideoPipeline(Conference *conference);
    void startSender();
    void startReceiver();

    // all public methods must be locked internally before use
    std::recursive_mutex mutex_ = {};

    std::unique_ptr<SocketPair> socketPair_ = nullptr;
    std::unique_ptr<VideoSender> sender_ = nullptr;
    std::unique_ptr<VideoReceiveThread> receiveThread_ = nullptr;
    std::map<std::string, std::string> txArgs_;
    std::map<std::string, std::string> rxArgs_ = {};
    bool sending_ = false;
    bool receiving_ = false;
    const std::string callID_;
    Conference* conference_ = nullptr;
    std::shared_ptr<VideoMixer> videoMixer_ = nullptr;
    std::shared_ptr<VideoFrameActiveWriter> videoLocal_ = nullptr;
};

}

#endif // __VIDEO_RTP_SESSION_H__
