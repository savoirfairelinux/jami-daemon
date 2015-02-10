/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
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

#ifndef AVFORMAT_RTP_SESSION_H__
#define AVFORMAT_RTP_SESSION_H__

#include "threadloop.h"
#include "media/rtp_session.h"
#include "media/audio/audiobuffer.h"
#include "noncopyable.h"

#include <map>
#include <string>
#include <memory>
#include <mutex>

namespace ring {

class RingBuffer;
class AudioSender;
class AudioReceiveThread;
class IceSocket;

class AVFormatRtpSession : public RtpSession {
public:
    AVFormatRtpSession(const std::string& id);
    virtual ~AVFormatRtpSession();

    void start(int localPort);
    void start(std::unique_ptr<IceSocket> rtp_sock, std::unique_ptr<IceSocket> rtcp_sock);
    void stop();

private:
    NON_COPYABLE(AVFormatRtpSession);

    void startSender();
    void startReceiver();

    std::unique_ptr<AudioSender> sender_;
    std::unique_ptr<AudioReceiveThread> receiveThread_;
    std::shared_ptr<RingBuffer> ringbuffer_;
};

} // namespace ring

#endif // __AVFORMAT_RTP_SESSION_H__
