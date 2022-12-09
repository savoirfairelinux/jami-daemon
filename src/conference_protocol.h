/*
 *  Copyright (C) 2022-2023 Savoir-faire Linux Inc.
 *
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string_view>
#include <functional>
#include <json/json.h>

#include "logger.h"

namespace jami {

/**
 * Used to parse confOrder objects
 * @note the user of this class must initialize the different lambdas.
 */
class ConfProtocolParser
{
public:
    ConfProtocolParser() {};

    void onVersion(std::function<void(uint32_t)>&& cb) { version_ = std::move(cb); }
    /**
     * Ask the caller to check if a peer is authorized (moderator of the conference)
     */
    void onCheckAuthorization(std::function<bool(std::string_view)>&& cb)
    {
        checkAuthorization_ = std::move(cb);
    }

    void onHangupParticipant(std::function<void(const std::string&, const std::string&)>&& cb)
    {
        hangupParticipant_ = std::move(cb);
    }
    void onRaiseHand(std::function<void(const std::string&, const std::string&, bool)>&& cb)
    {
        raiseHand_ = std::move(cb);
    }
    void onSetActiveStream(std::function<void(const std::string&, const std::string&, const std::string&, bool)>&& cb)
    {
        setActiveStream_ = std::move(cb);
    }
    void onMuteStreamAudio(
        std::function<void(const std::string&, const std::string&, const std::string&, bool)>&& cb)
    {
        muteStreamAudio_ = std::move(cb);
    }
    void onMuteStreamVideo(
        std::function<void(const std::string&, const std::string&, const std::string&, bool)>&& cb)
    {
        muteStreamVideo_ = std::move(cb);
    }
    void onSetLayout(std::function<void(int)>&& cb) { setLayout_ = std::move(cb); }

    // Version 0, deprecated
    void onKickParticipant(std::function<void(const std::string&)>&& cb)
    {
        kickParticipant_ = std::move(cb);
    }
    void onSetActiveParticipant(std::function<void(const std::string&)>&& cb)
    {
        setActiveParticipant_ = std::move(cb);
    }
    void onMuteParticipant(std::function<void(const std::string&, bool)>&& cb)
    {
        muteParticipant_ = std::move(cb);
    }
    void onRaiseHandUri(std::function<void(const std::string&, bool)>&& cb)
    {
        raiseHandUri_ = std::move(cb);
    }
    void onVoiceActivity(std::function<void(const std::string&, const std::string&, const std::string&, bool)>&& cb)
    {
        voiceActivity_ = std::move(cb);
    }

    /**
     * Inject in the parser the data to parse
     */
    void initData(Json::Value&& d, std::string peerId)
    {
        data_ = std::move(d);
        peerId_ = peerId;
    }

    /**
     * Parse the datas, this will call the methods injected if necessary
     */
    void parse();

private:
    void parseV0();
    void parseV1();

    std::string peerId_;
    Json::Value data_;

    std::function<void(uint32_t)> version_;

    std::function<bool(std::string_view)> checkAuthorization_;
    std::function<void(const std::string&, const std::string&)> hangupParticipant_;
    std::function<void(const std::string&, const std::string&, bool)> raiseHand_;
    std::function<void(const std::string&, const std::string&, const std::string&, bool)> setActiveStream_;
    std::function<void(const std::string&, const std::string&, const std::string&, bool)>
        muteStreamAudio_;
    std::function<void(const std::string&, const std::string&, const std::string&, bool)>
        muteStreamVideo_;
    std::function<void(int)> setLayout_;

    std::function<void(const std::string&, bool)> raiseHandUri_;
    std::function<void(const std::string&)> kickParticipant_;
    std::function<void(const std::string&)> setActiveParticipant_;
    std::function<void(const std::string&, bool)> muteParticipant_;
    std::function<void(const std::string&, const std::string&, const std::string&, bool)> voiceActivity_;
};

} // namespace jami
