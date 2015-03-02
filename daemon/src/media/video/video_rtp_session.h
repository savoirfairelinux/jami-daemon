/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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

#include "media/rtp_session.h"
#include "media/media_device.h"

#include "video_base.h"

#include <string>
#include <memory>

namespace ring {
class Conference;
} // namespace ring

namespace ring { namespace video {

class VideoMixer;
class VideoSender;
class VideoReceiveThread;

class VideoRtpSession : public RtpSession {
public:
    VideoRtpSession(const std::string& callID,
                    const DeviceParams& localVideoParams);
    ~VideoRtpSession();

    void start();
    void start(std::unique_ptr<IceSocket> rtp_sock,
               std::unique_ptr<IceSocket> rtcp_sock);
    void stop();

    void forceKeyFrame();
    void bindMixer(VideoMixer* mixer);
    void unbindMixer();
    void enterConference(Conference* conference);
    void exitConference();
    void switchInput(const DeviceParams& params) {
        localVideoParams_ = params;
    }

private:
    void setupConferenceVideoPipeline(Conference *conference);
    void startSender();
    void startReceiver();

    DeviceParams localVideoParams_;

    std::unique_ptr<VideoSender> sender_;
    std::unique_ptr<VideoReceiveThread> receiveThread_;
    Conference* conference_ {nullptr};
    std::shared_ptr<VideoMixer> videoMixer_;
    std::shared_ptr<VideoFrameActiveWriter> videoLocal_;
};

}} // namespace ring::video

#endif // __VIDEO_RTP_SESSION_H__
