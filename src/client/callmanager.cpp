/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#include "sip/sipvoiplink.h"
#include "audio/audiolayer.h"

#include "logger.h"
#include "manager.h"

#include "smartools.h"

namespace DRing {

void
registerCallHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    registerSignalHandlers(handlers);
}

std::string
placeCall(const std::string& accountId, const std::string& to)
{
    // Check if a destination number is available
    if (to.empty()) {
        JAMI_DBG("No number entered - Call stopped");
        return {};
    } else {
        return jami::Manager::instance().outgoingCall(accountId, to);
    }
}

std::string
placeCall(const std::string& accountId,
          const std::string& to,
          const std::map<std::string, std::string>& volatileCallDetails)
{
    // Check if a destination number is available
    if (to.empty()) {
        JAMI_DBG("No number entered - Call stopped");
        return {};
    } else {
        return jami::Manager::instance().outgoingCall(accountId, to, "", volatileCallDetails);
    }
}

std::string
placeCallWithMedia(const std::string& accountId,
                   const std::string& to,
                   const std::vector<DRing::MediaMap>& mediaList)
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
requestMediaChange(const std::string& accountId, const std::string& callId, const std::vector<DRing::MediaMap>& mediaList)
{
    return jami::Manager::instance().requestMediaChange(callId, mediaList);
}

bool
refuse(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().refuseCall(callId);
}

bool
accept(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().answerCall(callId);
}

bool
acceptWithMedia(const std::string& accountId, const std::string& callId, const std::vector<DRing::MediaMap>& mediaList)
{
    return jami::Manager::instance().answerCallWithMedia(callId, mediaList);
}

bool
answerMediaChangeRequest(const std::string& accountId, const std::string& callId, const std::vector<DRing::MediaMap>& mediaList)
{
    return jami::Manager::instance().answerMediaChangeRequest(callId, mediaList);
}

bool
hangUp(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().hangupCall(callId);
}

bool
hangUpConference(const std::string& confID)
{
    return jami::Manager::instance().hangupConference(confID);
}

bool
hold(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().onHoldCall(callId);
}

bool
unhold(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().offHoldCall(callId);
}

bool
muteLocalMedia(const std::string& accountId, const std::string& callid, const std::string& mediaType, bool mute)
{
    return jami::Manager::instance().muteMediaCall(callid, mediaType, mute);
}

bool
transfer(const std::string& accountId, const std::string& callId, const std::string& to)
{
    return jami::Manager::instance().transferCall(callId, to);
}

bool
attendedTransfer(const std::string& accountId, const std::string& transferID, const std::string& targetID)
{
    return jami::Manager::instance().attendedTransfer(transferID, targetID);
}

bool
joinParticipant(const std::string& sel_callId, const std::string& drag_callId)
{
    return jami::Manager::instance().joinParticipant(sel_callId, drag_callId);
}

void
createConfFromParticipantList(const std::vector<std::string>& participants)
{
    jami::Manager::instance().createConfFromParticipantList(participants);
}

void
setConferenceLayout(const std::string& confId, uint32_t layout)
{
    jami::Manager::instance().setConferenceLayout(confId, layout);
}

void
setActiveParticipant(const std::string& confId, const std::string& callId)
{
    jami::Manager::instance().setActiveParticipant(confId, callId);
}

bool
isConferenceParticipant(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().isConferenceParticipant(callId);
}

void
removeConference(const std::string& conference_id)
{
    jami::Manager::instance().removeConference(conference_id);
}

void
startSmartInfo(uint32_t refreshTimeMs)
{
    jami::Smartools::getInstance().start(std::chrono::milliseconds(refreshTimeMs));
}

void
stopSmartInfo()
{
    jami::Smartools::getInstance().stop();
}

bool
addParticipant(const std::string& callId, const std::string& confID)
{
    return jami::Manager::instance().addParticipant(callId, confID);
}

bool
addMainParticipant(const std::string& confID)
{
    return jami::Manager::instance().addMainParticipant(confID);
}

bool
detachLocalParticipant()
{
    return jami::Manager::instance().detachLocalParticipant();
}

bool
detachParticipant(const std::string& callId)
{
    return jami::Manager::instance().detachParticipant(callId);
}

bool
joinConference(const std::string& sel_confID, const std::string& drag_confID)
{
    return jami::Manager::instance().joinConference(sel_confID, drag_confID);
}

bool
holdConference(const std::string& confID)
{
    return jami::Manager::instance().holdConference(confID);
}

bool
unholdConference(const std::string& confID)
{
    return jami::Manager::instance().unHoldConference(confID);
}

std::map<std::string, std::string>
getConferenceDetails(const std::string& callId)
{
    return jami::Manager::instance().getConferenceDetails(callId);
}

std::vector<std::string>
getConferenceList()
{
    return jami::Manager::instance().getConferenceList();
}

std::vector<std::string>
getParticipantList(const std::string& confID)
{
    return jami::Manager::instance().getParticipantList(confID);
}

std::vector<std::string>
getDisplayNames(const std::string& confID)
{
    return jami::Manager::instance().getDisplayNames(confID);
}

std::string
getConferenceId(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().getConferenceId(callId);
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
    return jami::Manager::instance().toggleRecordingCall(callId);
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
    return jami::Manager::instance().isRecording(callId);
}

std::string
getCurrentAudioCodecName(const std::string&)
{
    JAMI_WARN("Deprecated");
    return "";
}

std::map<std::string, std::string>
getCallDetails(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().getCallDetails(callId);
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
getConferenceInfos(const std::string& confId)
{
    return jami::Manager::instance().getConferenceInfos(confId);
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
    return jami::Manager::instance().switchInput(callId, resource);
}

bool
switchSecondaryInput(const std::string& confId, const std::string& resource)
{
    if (auto conf = jami::Manager::instance().getConferenceFromID(confId)) {
        conf->switchSecondaryInput(resource);
        return true;
    }
    return false;
}

void
sendTextMessage(const std::string& accountId,
                const std::string& callId,
                const std::map<std::string, std::string>& messages,
                const std::string& from,
                bool isMixed)
{
    jami::runOnMainThread([callId, messages, from, isMixed] {
        jami::Manager::instance().sendCallTextMessage(callId, messages, from, isMixed);
    });
}

void
setModerator(const std::string& confId, const std::string& peerId, const bool& state)
{
    jami::Manager::instance().setModerator(confId, peerId, state);
}

void
muteParticipant(const std::string& confId, const std::string& peerId, const bool& state)
{
    jami::Manager::instance().muteParticipant(confId, peerId, state);
}

void
hangupParticipant(const std::string& confId, const std::string& participant)
{
    jami::Manager::instance().hangupParticipant(confId, participant);
}
} // namespace DRing
