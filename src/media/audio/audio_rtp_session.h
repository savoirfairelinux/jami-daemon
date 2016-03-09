/*
 *  Copyright (C) 2014-2016 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 */

#ifndef AUDIO_RTP_SESSION_H__
#define AUDIO_RTP_SESSION_H__

#include "threadloop.h"
#include "media/rtp_session.h"
#include "media/audio/audiobuffer.h"

#include <string>
#include <memory>

namespace ring {

class RingBuffer;
class AudioSender;
class AudioReceiveThread;
class IceSocket;

class AudioRtpSession : public RtpSession {
    public:
        AudioRtpSession(const std::string& id);
        virtual ~AudioRtpSession();

        void start(std::unique_ptr<IceSocket> rtp_sock = nullptr,
                   std::unique_ptr<IceSocket> rtcp_sock = nullptr) override;
        void restartSender() override;
        void stop() override;
        void setMuted(bool isMuted);

    private:
        void startSender();
        void startReceiver();

        std::unique_ptr<AudioSender> sender_;
        std::unique_ptr<AudioReceiveThread> receiveThread_;
        std::shared_ptr<RingBuffer> ringbuffer_;
        uint16_t initSeqVal_ = 0;
};

} // namespace ring

#endif // __AUDIO_RTP_SESSION_H__
