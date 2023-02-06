/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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
 */
#include <vector>
#include <cstring>

#include "callmanager_interface.h"
#include "call_factory.h"
#include "client/ring_signal.h"

#include "sip/siptransport.h"
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "audio/audiolayer.h"
#include "media/media_attribute.h"
#include "string_utils.h"

#include "logger.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"

namespace libjami {

void
registerCallHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    registerSignalHandlers(handlers);
}

std::string
placeCall(const std::string& accountId, const std::string& to)
{
    // TODO. Remove ASAP.
    JAMI_WARN("This API is deprecated, use placeCallWithMedia() instead");
    return placeCallWithMedia(accountId, to, {});
}

std::string
placeCallWithMedia(const std::string& accountId,
                   const std::string& to,
                   const std::vector<libjami::MediaMap>& mediaList)
{
    // Check if a destination number is available
    if (to.empty()) {
        JAMI_DBG("No number entered - Call aborted");
        return {};
    } else {
        return jami::Manager::instance().outgoingCall(accountId, to, mediaList);
    }
}

bool
requestMediaChange(const std::string& accountId,
                   const std::string& callId,
                   const std::vector<libjami::MediaMap>& mediaList)
{
    if (auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            return call->requestMediaChange(mediaList);
        } else if (auto conf = account->getConference(callId)) {
            return conf->requestMediaChange(mediaList);
        }
    }
    return false;
}

bool
refuse(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().refuseCall(accountId, callId);
}

bool
accept(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().answerCall(accountId, callId);
}

bool
acceptWithMedia(const std::string& accountId,
                const std::string& callId,
                const std::vector<libjami::MediaMap>& mediaList)
{
    return jami::Manager::instance().answerCall(accountId, callId, mediaList);
}

bool
answerMediaChangeRequest(const std::string& accountId,
                         const std::string& callId,
                         const std::vector<libjami::MediaMap>& mediaList)
{
    if (auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(callId)) {
            try {
                call->answerMediaChangeRequest(mediaList);
                return true;
            } catch (const std::runtime_error& e) {
                JAMI_ERR("%s", e.what());
            }
        }
    return false;
}

bool
hangUp(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().hangupCall(accountId, callId);
}

bool
hangUpConference(const std::string& accountId, const std::string& confId)
{
    return jami::Manager::instance().hangupConference(accountId, confId);
}

bool
hold(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().onHoldCall(accountId, callId);
}

bool
unhold(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().offHoldCall(accountId, callId);
}

bool
muteLocalMedia(const std::string& accountId,
               const std::string& callId,
               const std::string& mediaType,
               bool mute)
{
    if (auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            JAMI_DBG("Muting [%s] for call %s", mediaType.c_str(), callId.c_str());
            call->muteMedia(mediaType, mute);
            return true;
        } else if (auto conf = account->getConference(callId)) {
            JAMI_DBG("Muting local host [%s] for conference %s", mediaType.c_str(), callId.c_str());
            conf->muteLocalHost(mute, mediaType);
            return true;
        } else {
            JAMI_WARN("ID %s doesn't match any call or conference", callId.c_str());
        }
    }
    return false;
}

bool
transfer(const std::string& accountId, const std::string& callId, const std::string& to)
{
    return jami::Manager::instance().transferCall(accountId, callId, to);
}

bool
attendedTransfer(const std::string& accountId,
                 const std::string& transferID,
                 const std::string& targetID)
{
    if (auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(transferID))
            return call->attendedTransfer(targetID);
    return false;
}

bool
joinParticipant(const std::string& accountId,
                const std::string& sel_callId,
                const std::string& account2Id,
                const std::string& drag_callId)
{
    return jami::Manager::instance().joinParticipant(accountId, sel_callId, account2Id, drag_callId);
}

void
createConfFromParticipantList(const std::string& accountId,
                              const std::vector<std::string>& participants)
{
    jami::Manager::instance().createConfFromParticipantList(accountId, participants);
}

void
setConferenceLayout(const std::string& accountId, const std::string& confId, uint32_t layout)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->setLayout(layout);
        } else if (auto call = account->getCall(confId)) {
            Json::Value root;
            root["layout"] = layout;
            call->sendConfOrder(root);
        }
    }
}

bool
isConferenceParticipant(const std::string& accountId, const std::string& callId)
{
    if (auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(callId))
            return call->isConferenceParticipant();
    return false;
}

void
startSmartInfo(uint32_t refreshTimeMs)
{
    JAMI_WARNING("startSmartInfo is deprecated and does nothing.");
}

void
stopSmartInfo()
{
    JAMI_WARNING("stopSmartInfo is deprecated and does nothing.");
}

bool
addParticipant(const std::string& accountId,
               const std::string& callId,
               const std::string& account2Id,
               const std::string& confId)
{
    return jami::Manager::instance().addParticipant(accountId, callId, account2Id, confId);
}

bool
addMainParticipant(const std::string& accountId, const std::string& confId)
{
    return jami::Manager::instance().addMainParticipant(accountId, confId);
}

bool
detachLocalParticipant()
{
    return jami::Manager::instance().detachLocalParticipant();
}

bool
detachParticipant(const std::string&, const std::string& callId)
{
    return jami::Manager::instance().detachParticipant(callId);
}

bool
joinConference(const std::string& accountId,
               const std::string& sel_confId,
               const std::string& account2Id,
               const std::string& drag_confId)
{
    return jami::Manager::instance().joinConference(accountId, sel_confId, account2Id, drag_confId);
}

bool
holdConference(const std::string& accountId, const std::string& confId)
{
    /*
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        if (auto conf = account->getConference(confId)) {
            conf->detach();
            jami::emitSignal<libjami::CallSignal::ConferenceChanged>(conf->getConfId(),
    conf->getStateStr()); return true;
        }
    return false;*/
    return jami::Manager::instance().holdConference(accountId, confId);
}

bool
unholdConference(const std::string& accountId, const std::string& confId)
{
    return jami::Manager::instance().unHoldConference(accountId, confId);
}

std::map<std::string, std::string>
getConferenceDetails(const std::string& accountId, const std::string& confId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        if (auto conf = account->getConference(confId))
            return {{"ID", confId},
                    {"STATE", conf->getStateStr()},
#ifdef ENABLE_VIDEO
                    {"VIDEO_SOURCE", conf->getVideoInput()},
#endif
                    {"RECORDING", conf->isRecording() ? jami::TRUE_STR : jami::FALSE_STR}};
    return {};
}

std::vector<std::map<std::string, std::string>>
currentMediaList(const std::string& accountId, const std::string& callId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            return call->currentMediaList();
        } else if (auto conf = account->getConference(callId)) {
            return conf->currentMediaList();
        }
    }
    JAMI_WARN("Call not found %s", callId.c_str());
    return {};
}

std::vector<std::string>
getConferenceList(const std::string& accountId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        return account->getConferenceList();
    return {};
}

std::vector<std::string>
getParticipantList(const std::string& accountId, const std::string& confId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        if (auto conf = account->getConference(confId)) {
            const auto& participants(conf->getParticipantList());
            return {participants.begin(), participants.end()};
        }
    return {};
}

std::string
getConferenceId(const std::string& accountId, const std::string& callId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(callId))
            if (auto conf = call->getConference())
                return conf->getConfId();
    return {};
}

bool
startRecordedFilePlayback(const std::string& filepath)
{
    return jami::Manager::instance().startRecordedFilePlayback(filepath);
}

void
stopRecordedFilePlayback()
{
    jami::Manager::instance().stopRecordedFilePlayback();
}

bool
toggleRecording(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().toggleRecordingCall(accountId, callId);
}

void
setRecording(const std::string& accountId, const std::string& callId)
{
    toggleRecording(accountId, callId);
}

void
recordPlaybackSeek(double value)
{
    jami::Manager::instance().recordingPlaybackSeek(value);
}

bool
getIsRecording(const std::string& accountId, const std::string& callId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            return call->isRecording();
        } else if (auto conf = account->getConference(callId)) {
            return conf->isRecording();
        }
    }
    return false;
}

std::map<std::string, std::string>
getCallDetails(const std::string& accountId, const std::string& callId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(callId))
            return call->getDetails();
    return {};
}

std::vector<std::string>
getCallList()
{
    return jami::Manager::instance().getCallList();
}

std::vector<std::string>
getCallList(const std::string& accountId)
{
    if (accountId.empty())
        return jami::Manager::instance().getCallList();
    else if (const auto account = jami::Manager::instance().getAccount(accountId))
        return account->getCallList();
    JAMI_WARN("Unknown account: %s", accountId.c_str());
    return {};
}

std::vector<std::map<std::string, std::string>>
getConferenceInfos(const std::string& accountId, const std::string& confId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId))
            return conf->getConferenceInfos();
        else if (auto call = account->getCall(confId))
            return call->getConferenceInfos();
    }
    return {};
}

void
playDTMF(const std::string& key)
{
    auto code = key.data()[0];
    jami::Manager::instance().playDtmf(code);

    if (auto current_call = jami::Manager::instance().getCurrentCall())
        current_call->carryingDTMFdigits(code);
}

void
startTone(int32_t start, int32_t type)
{
    if (start) {
        if (type == 0)
            jami::Manager::instance().playTone();
        else
            jami::Manager::instance().playToneWithMessage();
    } else
        jami::Manager::instance().stopTone();
}

bool
switchInput(const std::string& accountId, const std::string& callId, const std::string& resource)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(callId)) {
            conf->switchInput(resource);
            return true;
        } else if (auto call = account->getCall(callId)) {
            call->switchInput(resource);
            return true;
        }
    }
    return false;
}

bool
switchSecondaryInput(const std::string& accountId,
                     const std::string& confId,
                     const std::string& resource)
{
    JAMI_ERR("Use requestMediaChange");
    return false;
}

void
sendTextMessage(const std::string& accountId,
                const std::string& callId,
                const std::map<std::string, std::string>& messages,
                const std::string& from,
                bool isMixed)
{
    jami::runOnMainThread([accountId, callId, messages, from, isMixed] {
        jami::Manager::instance().sendCallTextMessage(accountId, callId, messages, from, isMixed);
    });
}

void
setModerator(const std::string& accountId,
             const std::string& confId,
             const std::string& peerId,
             const bool& state)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->setModerator(peerId, state);
        } else {
            JAMI_WARN("Fail to change moderator %s, conference %s not found",
                      peerId.c_str(),
                      confId.c_str());
        }
    }
}

void
muteParticipant(const std::string& accountId,
                const std::string& confId,
                const std::string& peerId,
                const bool& state)
{
    JAMI_ERR() << "muteParticipant is deprecated, please use muteStream";
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->muteParticipant(peerId, state);
        } else if (auto call = account->getCall(confId)) {
            Json::Value root;
            root["muteParticipant"] = peerId;
            root["muteState"] = state ? jami::TRUE_STR : jami::FALSE_STR;
            call->sendConfOrder(root);
        }
    }
}

void
muteStream(const std::string& accountId,
           const std::string& confId,
           const std::string& accountUri,
           const std::string& deviceId,
           const std::string& streamId,
           const bool& state)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->muteStream(accountUri, deviceId, streamId, state);
        } else if (auto call = account->getCall(confId)) {
            if (call->conferenceProtocolVersion() == 1) {
                Json::Value sinkVal;
                sinkVal["muteAudio"] = state;
                Json::Value mediasObj;
                mediasObj[streamId] = sinkVal;
                Json::Value deviceVal;
                deviceVal["medias"] = mediasObj;
                Json::Value deviceObj;
                deviceObj[deviceId] = deviceVal;
                Json::Value accountVal;
                deviceVal["devices"] = deviceObj;
                Json::Value root;
                root[accountUri] = deviceVal;
                root["version"] = 1;
                call->sendConfOrder(root);
            } else if (call->conferenceProtocolVersion() == 0) {
                Json::Value root;
                root["muteParticipant"] = accountUri;
                root["muteState"] = state ? jami::TRUE_STR : jami::FALSE_STR;
                call->sendConfOrder(root);
            }
        }
    }
}

void
setActiveParticipant(const std::string& accountId,
                     const std::string& confId,
                     const std::string& participant)
{
    JAMI_ERR() << "setActiveParticipant is deprecated, please use setActiveStream";
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->setActiveParticipant(participant);
        } else if (auto call = account->getCall(confId)) {
            Json::Value root;
            root["activeParticipant"] = participant;
            call->sendConfOrder(root);
        }
    }
}

void
setActiveStream(const std::string& accountId,
                const std::string& confId,
                const std::string& accountUri,
                const std::string& deviceId,
                const std::string& streamId,
                const bool& state)
{
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->setActiveStream(streamId, state);
        } else if (auto call = account->getCall(confId)) {
            if (call->conferenceProtocolVersion() == 1) {
                Json::Value sinkVal;
                sinkVal["active"] = state;
                Json::Value mediasObj;
                mediasObj[streamId] = sinkVal;
                Json::Value deviceVal;
                deviceVal["medias"] = mediasObj;
                Json::Value deviceObj;
                deviceObj[deviceId] = deviceVal;
                Json::Value accountVal;
                deviceVal["devices"] = deviceObj;
                Json::Value root;
                root[accountUri] = deviceVal;
                root["version"] = 1;
                call->sendConfOrder(root);
            } else if (call->conferenceProtocolVersion() == 0) {
                Json::Value root;
                root["activeParticipant"] = accountUri;
                call->sendConfOrder(root);
            }
        }
    }
}

void
hangupParticipant(const std::string& accountId,
                  const std::string& confId,
                  const std::string& accountUri,
                  const std::string& deviceId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->hangupParticipant(accountUri, deviceId);
        } else if (auto call = std::static_pointer_cast<jami::SIPCall>(account->getCall(confId))) {
            if (call->conferenceProtocolVersion() == 1) {
                Json::Value deviceVal;
                deviceVal["hangup"] = jami::TRUE_STR;
                Json::Value deviceObj;
                deviceObj[deviceId] = deviceVal;
                Json::Value accountVal;
                deviceVal["devices"] = deviceObj;
                Json::Value root;
                root[accountUri] = deviceVal;
                root["version"] = 1;
                call->sendConfOrder(root);
            } else if (call->conferenceProtocolVersion() == 0) {
                Json::Value root;
                root["hangupParticipant"] = accountUri;
                call->sendConfOrder(root);
            }
        }
    }
}

void
raiseParticipantHand(const std::string& accountId,
                     const std::string& confId,
                     const std::string& peerId,
                     const bool& state)
{
    JAMI_ERR() << "raiseParticipantHand is deprecated, please use raiseHand";
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            if (auto call = std::static_pointer_cast<jami::SIPCall>(
                    conf->getCallFromPeerID(peerId))) {
                if (auto* transport = call->getTransport())
                    conf->setHandRaised(std::string(transport->deviceId()), state);
            }
        } else if (auto call = account->getCall(confId)) {
            Json::Value root;
            root["handRaised"] = peerId;
            root["handState"] = state ? jami::TRUE_STR : jami::FALSE_STR;
            call->sendConfOrder(root);
        }
    }
}

void
raiseHand(const std::string& accountId,
          const std::string& confId,
          const std::string& accountUri,
          const std::string& deviceId,
          const bool& state)
{
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        if (auto conf = account->getConference(confId)) {
            auto device = deviceId;
            if (device.empty())
                device = std::string(account->currentDeviceId());
            conf->setHandRaised(device, state);
        } else if (auto call = std::static_pointer_cast<jami::SIPCall>(account->getCall(confId))) {
            if (call->conferenceProtocolVersion() == 1) {
                Json::Value deviceVal;
                deviceVal["raiseHand"] = state;
                Json::Value deviceObj;
                std::string device = deviceId.empty() ? std::string(account->currentDeviceId())
                                                      : deviceId;
                deviceObj[device] = deviceVal;
                Json::Value accountVal;
                deviceVal["devices"] = deviceObj;
                Json::Value root;
                std::string uri = accountUri.empty() ? account->getUsername() : accountUri;
                root[uri] = deviceVal;
                root["version"] = 1;
                call->sendConfOrder(root);
            } else if (call->conferenceProtocolVersion() == 0) {
                Json::Value root;
                root["handRaised"] = account->getUsername();
                root["handState"] = state ? jami::TRUE_STR : jami::FALSE_STR;
                call->sendConfOrder(root);
            }
        }
    }
}

} // namespace libjami
