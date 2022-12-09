/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#pragma once

#include "socket_pair.h"
#include "connectivity/sip_utils.h"
#include "media/media_codec.h"

#include <functional>
#include <string>
#include <memory>
#include <mutex>

namespace jami {

class MediaRecorder;

class RtpSession
{
public:
    // Media direction
    enum class Direction { SEND, RECV };

    // Note: callId is used for ring buffers and smarttools
    RtpSession(const std::string& callId, const std::string& streamId, MediaType type)
        : callId_(callId)
        , streamId_(streamId)
        , mediaType_(type)
    {}
    virtual ~RtpSession() {};

    virtual void start(std::unique_ptr<dhtnet::IceSocket> rtp_sock, std::unique_ptr<dhtnet::IceSocket> rtcp_sock) = 0;
    virtual void restartSender() = 0;
    virtual void stop() = 0;
    void setMediaSource(const std::string& resource) { input_ = resource; }
    const std::string& getInput() const { return input_; }
    MediaType getMediaType() const { return mediaType_; };
    virtual void setMuted(bool mute, Direction dir = Direction::SEND) = 0;

    virtual void updateMedia(const MediaDescription& send, const MediaDescription& receive)
    {
        send_ = send;
        receive_ = receive;
    }

    void setMtu(uint16_t mtu) { mtu_ = mtu; }

    void setSuccessfulSetupCb(const std::function<void(MediaType, bool)>& cb)
    {
        onSuccessfulSetup_ = cb;
    }

    virtual void initRecorder() = 0;
    virtual void deinitRecorder() = 0;
    std::shared_ptr<SystemCodecInfo> getCodec() const { return send_.codec; }
    const dhtnet::IpAddr& getSendAddr() const { return send_.addr; };
    const dhtnet::IpAddr& getRecvAddr() const { return receive_.addr; };

    inline std::string streamId() const { return streamId_; }

protected:
    std::recursive_mutex mutex_;
    const std::string callId_;
    const std::string streamId_;
    MediaType mediaType_;
    std::unique_ptr<SocketPair> socketPair_;
    std::string input_ {};
    MediaDescription send_;
    MediaDescription receive_;
    uint16_t mtu_;
    std::mutex recorderMtx_;
    std::shared_ptr<MediaRecorder> recorder_;
    std::function<void(MediaType, bool)> onSuccessfulSetup_;

    std::string getRemoteRtpUri() const { return "rtp://" + send_.addr.toString(true); }
};

} // namespace jami
