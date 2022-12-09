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

#include "conference_protocol.h"

#include "string_utils.h"

namespace jami {

namespace ProtocolKeys {

constexpr static const char* PROTOVERSION = "version";
constexpr static const char* LAYOUT = "layout";
// V0
constexpr static const char* HANDRAISED = "handRaised";
constexpr static const char* HANDSTATE = "handState";
constexpr static const char* ACTIVEPART = "activeParticipant";
constexpr static const char* MUTEPART = "muteParticipant";
constexpr static const char* MUTESTATE = "muteState";
constexpr static const char* HANGUPPART = "hangupParticipant";
// V1
constexpr static const char* DEVICES = "devices";
constexpr static const char* MEDIAS = "medias";
constexpr static const char* RAISEHAND = "raiseHand";
constexpr static const char* HANGUP = "hangup";
constexpr static const char* ACTIVE = "active";
constexpr static const char* MUTEAUDIO = "muteAudio";
// Future
constexpr static const char* MUTEVIDEO = "muteVideo";
constexpr static const char* VOICEACTIVITY = "voiceActivity";

} // namespace ProtocolKeys

void
ConfProtocolParser::parse()
{
    if (data_.isMember(ProtocolKeys::PROTOVERSION)) {
        uint32_t version = data_[ProtocolKeys::PROTOVERSION].asUInt();
        if (version_)
            version_(version);
        if (version == 1) {
            parseV1();
        } else {
            JAMI_WARN() << "Unsupported protocol version " << version;
        }
    } else {
        parseV0();
    }
}

void
ConfProtocolParser::parseV0()
{
    if (!checkAuthorization_ || !raiseHandUri_ || !setLayout_ || !setActiveParticipant_
        || !muteParticipant_ || !kickParticipant_) {
        JAMI_ERR() << "Missing methods for ConfProtocolParser";
        return;
    }
    // Check if all lambdas set
    auto isPeerModerator = checkAuthorization_(peerId_);
    if (data_.isMember(ProtocolKeys::HANDRAISED)) {
        auto state = data_[ProtocolKeys::HANDSTATE].asString() == TRUE_STR;
        auto uri = data_[ProtocolKeys::HANDRAISED].asString();
        if (peerId_ == uri) {
            // In this case, the user want to change their state
            raiseHandUri_(uri, state);
        } else if (!state && isPeerModerator) {
            // In this case a moderator can lower the hand
            raiseHandUri_(uri, state);
        }
    }
    if (!isPeerModerator) {
        JAMI_WARN("Received conference order from a non master (%.*s)",
                  (int) peerId_.size(),
                  peerId_.data());
        return;
    }
    if (data_.isMember(ProtocolKeys::LAYOUT)) {
        setLayout_(data_[ProtocolKeys::LAYOUT].asInt());
    }
    if (data_.isMember(ProtocolKeys::ACTIVEPART)) {
        setActiveParticipant_(data_[ProtocolKeys::ACTIVEPART].asString());
    }
    if (data_.isMember(ProtocolKeys::MUTEPART) && data_.isMember(ProtocolKeys::MUTESTATE)) {
        muteParticipant_(data_[ProtocolKeys::MUTEPART].asString(),
                         data_[ProtocolKeys::MUTESTATE].asString() == TRUE_STR);
    }
    if (data_.isMember(ProtocolKeys::HANGUPPART)) {
        kickParticipant_(data_[ProtocolKeys::HANGUPPART].asString());
    }
}

void
ConfProtocolParser::parseV1()
{
    if (!checkAuthorization_ || !setLayout_ || !raiseHand_ || !hangupParticipant_
        || !muteStreamAudio_ || !setActiveStream_) {
        JAMI_ERR() << "Missing methods for ConfProtocolParser";
        return;
    }

    auto isPeerModerator = checkAuthorization_(peerId_);
    for (Json::Value::const_iterator itr = data_.begin(); itr != data_.end(); itr++) {
        auto key = itr.key();
        if (isPeerModerator && key == ProtocolKeys::LAYOUT) {
            // Note: can be removed soon
            setLayout_(itr->asInt());
        } else {
            auto accValue = *itr;
            if (accValue.isMember(ProtocolKeys::DEVICES)) {
                auto accountUri = key.asString();
                for (Json::Value::const_iterator itrd = accValue[ProtocolKeys::DEVICES].begin();
                     itrd != accValue[ProtocolKeys::DEVICES].end();
                     itrd++) {
                    auto deviceId = itrd.key().asString();
                    auto deviceValue = *itrd;
                    if (deviceValue.isMember(ProtocolKeys::RAISEHAND)) {
                        auto newState = deviceValue[ProtocolKeys::RAISEHAND].asBool();
                        if (peerId_ == accountUri || (!newState && isPeerModerator)) {
                            raiseHand_(peerId_, deviceId, newState);
                        }
                    }
                    if (isPeerModerator && deviceValue.isMember(ProtocolKeys::HANGUP)) {
                        hangupParticipant_(accountUri, deviceId);
                    }
                    if (deviceValue.isMember(ProtocolKeys::MEDIAS)) {
                        for (Json::Value::const_iterator itrm = accValue[ProtocolKeys::MEDIAS]
                                                                    .begin();
                             itrm != accValue[ProtocolKeys::MEDIAS].end();
                             itrm++) {
                            auto streamId = itrm.key().asString();
                            auto mediaVal = *itrm;
                            if (mediaVal.isMember(ProtocolKeys::VOICEACTIVITY)) {
                                voiceActivity_(peerId_, deviceId, streamId,
                                               mediaVal[ProtocolKeys::VOICEACTIVITY].asBool());
                            }
                            if (isPeerModerator) {
                                if (mediaVal.isMember(ProtocolKeys::MUTEVIDEO)
                                    && !muteStreamVideo_) {
                                    // Note: For now, it's not implemented so not set
                                    muteStreamVideo_(accountUri,
                                                     deviceId,
                                                     streamId,
                                                     mediaVal[ProtocolKeys::MUTEVIDEO].asBool());
                                }
                                if (mediaVal.isMember(ProtocolKeys::MUTEAUDIO)) {
                                    muteStreamAudio_(accountUri,
                                                     deviceId,
                                                     streamId,
                                                     mediaVal[ProtocolKeys::MUTEAUDIO].asBool());
                                }
                                if (mediaVal.isMember(ProtocolKeys::ACTIVE)) {
                                    setActiveStream_(accountUri, deviceId, streamId,
                                                     mediaVal[ProtocolKeys::ACTIVE].asBool());
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace jami
