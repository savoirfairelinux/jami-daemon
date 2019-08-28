/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
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

#include "audiobuffer.h"
#include "media_device.h"
#include "rtp_session.h"
#include "threadloop.h"

#include <string>
#include <memory>

namespace jami {

class AudioInput;
class AudioReceiveThread;
class AudioSender;
class IceSocket;
class MediaRecorder;
class RingBuffer;

class AudioRtpSession : public RtpSession {
    public:
        AudioRtpSession(const std::string& id);
        virtual ~AudioRtpSession();

        void start(std::string peerUri,
                   std::unique_ptr<IceSocket> rtp_sock,
                   std::unique_ptr<IceSocket> rtcp_sock) override;
        void restartSender() override;
        void stop() override;
        void setMuted(bool isMuted);

        void switchInput(const std::string& resource) { input_ = resource; }

        void initRecorder(std::shared_ptr<MediaRecorder>& rec) override;
        void deinitRecorder(std::shared_ptr<MediaRecorder>& rec) override;

    private:
        void startSender();
        void startReceiver(std::string peerUri);

        std::unique_ptr<AudioSender> sender_;
        std::unique_ptr<AudioReceiveThread> receiveThread_;
        std::shared_ptr<AudioInput> audioInput_;
        std::shared_ptr<RingBuffer> ringbuffer_;
        uint16_t initSeqVal_ = 0;
        bool muteState_ = false;
        DeviceParams localAudioParams_;
        std::string input_;
};

} // namespace jami

#endif // __AUDIO_RTP_SESSION_H__
