/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "media/media_device.h"
#include "media/rtp_session.h"
#include "media/media_stream.h"

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

struct RTCPInfo
{
    float packetLoss;
    unsigned int jitter;
    unsigned int nb_sample;
    float latency;
};

class AudioRtpSession : public RtpSession, public std::enable_shared_from_this<AudioRtpSession>
{
public:
    AudioRtpSession(const std::string& callId,
                    const std::string& streamId,
                    const std::shared_ptr<MediaRecorder>& rec);
    virtual ~AudioRtpSession();

    void start(std::unique_ptr<dhtnet::IceSocket> rtp_sock, std::unique_ptr<dhtnet::IceSocket> rtcp_sock) override;
    void restartSender() override;
    void stop() override;
    void setMuted(bool muted, Direction dir = Direction::SEND) override;

    void initRecorder() override;
    void deinitRecorder() override;

    std::shared_ptr<AudioInput>& getAudioLocal() { return audioInput_; }
    std::unique_ptr<AudioReceiveThread>& getAudioReceive() { return receiveThread_; }

    void setVoiceCallback(std::function<void(bool)> cb);

private:
    void startSender();
    void startReceiver();
    bool check_RCTP_Info_RR(RTCPInfo& rtcpi);
    void adaptQualityAndBitrate();
    void dropProcessing(RTCPInfo* rtcpi);
    void setNewPacketLoss(unsigned int newPL);
    float getPonderateLoss(float lastLoss);

    std::unique_ptr<AudioSender> sender_;
    std::unique_ptr<AudioReceiveThread> receiveThread_;
    std::shared_ptr<AudioInput> audioInput_;
    std::shared_ptr<RingBuffer> ringbuffer_;
    uint16_t initSeqVal_ {0};
    bool muteState_ {false};
    unsigned packetLoss_ {10};
    DeviceParams localAudioParams_;

    InterruptedThreadLoop rtcpCheckerThread_;
    void processRtcpChecker();

    // Interval in seconds between RTCP checking
    std::chrono::seconds rtcp_checking_interval {4};

    std::function<void(bool)> voiceCallback_;

    void attachRemoteRecorder(const MediaStream& ms);
    void attachLocalRecorder(const MediaStream& ms);
};

} // namespace jami
