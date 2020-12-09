/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "sip/sip_utils.h"
#include "media/media_codec.h"

#include <functional>
#include <string>
#include <memory>
#include <mutex>

namespace jami {

class IceSocket;
class MediaRecorder;

class RtpSession
{
public:
    RtpSession(const std::string& callID, MediaType type)
        : callID_(callID)
        , mediaType_(type)
    {}
    virtual ~RtpSession() {};

    virtual void start(std::unique_ptr<IceSocket> rtp_sock, std::unique_ptr<IceSocket> rtcp_sock) = 0;
    virtual void restartSender() = 0;
    virtual void stop() = 0;
    void setMediaSource(const std::string& resource) { input_ = resource; }
    const std::string& getInput() const { return input_; }
    MediaType getMediaType() const { return mediaType_; };
    virtual void setMuted(bool mute) { muteState_ = mute; };
    virtual void updateMedia(const MediaDescription& send, const MediaDescription& receive)
    {
        send_ = send;
        receive_ = receive;
    }

    bool isSending() const noexcept { return send_.enabled; }
    bool isReceiving() const noexcept { return receive_.enabled; }

    void setMtu(uint16_t mtu) { mtu_ = mtu; }

    void setSuccessfulSetupCb(const std::function<void(MediaType, bool)>& cb)
    {
        onSuccessfulSetup_ = cb;
    }

    virtual void initRecorder(std::shared_ptr<MediaRecorder>& rec) = 0;
    virtual void deinitRecorder(std::shared_ptr<MediaRecorder>& rec) = 0;
    std::shared_ptr<AccountCodecInfo> getCodec() const { return send_.codec; }
    const IpAddr& getSendAddr() const { return send_.addr; };
    const IpAddr& getRecvAddr() const { return receive_.addr; };

protected:
    std::recursive_mutex mutex_;
    const std::string callID_;
    MediaType mediaType_;
    std::unique_ptr<SocketPair> socketPair_;
    std::string input_ {};
    MediaDescription send_;
    MediaDescription receive_;
    uint16_t mtu_;
    bool muteState_ {false};

    std::function<void(MediaType, bool)> onSuccessfulSetup_;

    std::string getRemoteRtpUri() const { return "rtp://" + send_.addr.toString(true); }
};

} // namespace jami
