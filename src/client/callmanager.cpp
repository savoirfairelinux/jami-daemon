/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#include "sip/sipcall.h"
#include "sip/sipvoiplink.h"
#include "audio/audiolayer.h"

#include "logger.h"
#include "manager.h"

#include "smartools.h"

namespace DRing {

void
registerCallHandlers(const std::map<std::string,
    std::shared_ptr<CallbackWrapperBase>>&handlers)
{
    registerSignalHandlers(handlers);
}

std::string
placeCall(const std::string& accountID, const std::string& to)
{
    // Check if a destination number is available
    if (to.empty()) {
        JAMI_DBG("No number entered - Call stopped");
        return {};
    } else {
        return jami::Manager::instance().outgoingCall(accountID, to);
    }
}

std::string
placeCall(const std::string& accountID, const std::string& to, const std::map<std::string, std::string>& volatileCallDetails)
{
    // Check if a destination number is available
    if (to.empty()) {
        JAMI_DBG("No number entered - Call stopped");
        return {};
    } else {
        return jami::Manager::instance().outgoingCall(accountID, to, "", volatileCallDetails);
    }
}

bool
refuse(const std::string& callID)
{
    return jami::Manager::instance().refuseCall(callID);
}

bool
accept(const std::string& callID)
{
    return jami::Manager::instance().answerCall(callID);
}

bool
hangUp(const std::string& callID)
{
    return jami::Manager::instance().hangupCall(callID);
}

bool
hangUpConference(const std::string& confID)
{
    return jami::Manager::instance().hangupConference(confID);
}

bool
hold(const std::string& callID)
{
    return jami::Manager::instance().onHoldCall(callID);
}

bool
unhold(const std::string& callID)
{
    return jami::Manager::instance().offHoldCall(callID);
}

bool
muteLocalMedia(const std::string& callid, const std::string& mediaType, bool mute)
{
    return jami::Manager::instance().muteMediaCall(callid, mediaType, mute);
}

bool
transfer(const std::string& callID, const std::string& to)
{
    return jami::Manager::instance().transferCall(callID, to);
}

bool
attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    return jami::Manager::instance().attendedTransfer(transferID, targetID);
}

bool
joinParticipant(const std::string& sel_callID,
                             const std::string& drag_callID)
{
    return jami::Manager::instance().joinParticipant(sel_callID, drag_callID);
}

void
createConfFromParticipantList(const std::vector<std::string>& participants)
{
   jami::Manager::instance().createConfFromParticipantList(participants);
}

bool
isConferenceParticipant(const std::string& callID)
{
    return jami::Manager::instance().isConferenceParticipant(callID);
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
addParticipant(const std::string& callID, const std::string& confID)
{
    return jami::Manager::instance().addParticipant(callID, confID);
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
detachParticipant(const std::string& callID)
{
    return jami::Manager::instance().detachParticipant(callID);
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
getConferenceDetails(const std::string& callID)
{
    return jami::Manager::instance().getConferenceDetails(callID);
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
getConferenceId(const std::string& callID)
{
    return jami::Manager::instance().getConferenceId(callID);
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
toggleRecording(const std::string& callID)
{
    return jami::Manager::instance().toggleRecordingCall(callID);
}

void
setRecording(const std::string& callID)
{
    toggleRecording(callID);
}

void
recordPlaybackSeek(double value)
{
   jami::Manager::instance().recordingPlaybackSeek(value);
}

bool
getIsRecording(const std::string& callID)
{
    return jami::Manager::instance().isRecording(callID);
}

std::string
getCurrentAudioCodecName(const std::string&)
{
    JAMI_WARN("Deprecated");
    return "";
}

std::map<std::string, std::string>
getCallDetails(const std::string& callID)
{
    return jami::Manager::instance().getCallDetails(callID);
}

std::vector<std::string>
getCallList()
{
    return jami::Manager::instance().getCallList();
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
switchInput(const std::string& callID, const std::string& resource)
{
    return jami::Manager::instance().switchInput(callID, resource);
}

void
sendTextMessage(const std::string& callID, const std::map<std::string, std::string>& messages, const std::string& from, bool isMixed)
{
    jami::runOnMainThread([callID, messages, from, isMixed]{
        jami::Manager::instance().sendCallTextMessage(callID, messages, from, isMixed);
    });
}

} // namespace DRing
