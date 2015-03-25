/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */
#include <vector>
#include <cstring>

#include "callmanager_interface.h"
#include "call_factory.h"
#include "client/signal.h"

#include "sip/sipcall.h"
#include "sip/sipvoiplink.h"
#include "audio/audiolayer.h"

#include "logger.h"
#include "manager.h"

namespace DRing {

void
registerCallHandlers(const std::map<std::string,
                     std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    auto& handlers_ = ring::getSignalHandlers();
    for (auto& item : handlers) {
        auto iter = handlers_.find(item.first);
        if (iter == handlers_.end()) {
            RING_ERR("Signal %s not supported", item.first.c_str());
            continue;
        }

        iter->second = std::move(item.second);
    }
}

std::string
placeCall(const std::string& accountID, const std::string& to)
{
    // Check if a destination number is available
    if (to.empty()) {
        RING_DBG("No number entered - Call stopped");
        return {};
    } else {
        return ring::Manager::instance().outgoingCall(accountID, to);
    }
}

bool
refuse(const std::string& callID)
{
    return ring::Manager::instance().refuseCall(callID);
}

bool
accept(const std::string& callID)
{
    return ring::Manager::instance().answerCall(callID);
}

bool
hangUp(const std::string& callID)
{
    return ring::Manager::instance().hangupCall(callID);
}

bool
hangUpConference(const std::string& confID)
{
    return ring::Manager::instance().hangupConference(confID);
}

bool
hold(const std::string& callID)
{
    return ring::Manager::instance().onHoldCall(callID);
}

bool
unhold(const std::string& callID)
{
    return ring::Manager::instance().offHoldCall(callID);
}

bool
transfer(const std::string& callID, const std::string& to)
{
    return ring::Manager::instance().transferCall(callID, to);
}

bool
attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    return ring::Manager::instance().attendedTransfer(transferID, targetID);
}

bool
joinParticipant(const std::string& sel_callID,
                             const std::string& drag_callID)
{
    return ring::Manager::instance().joinParticipant(sel_callID, drag_callID);
}

void
createConfFromParticipantList(const std::vector<std::string>& participants)
{
   ring::Manager::instance().createConfFromParticipantList(participants);
}

bool
isConferenceParticipant(const std::string& callID)
{
    return ring::Manager::instance().isConferenceParticipant(callID);
}

void
removeConference(const std::string& conference_id)
{
   ring::Manager::instance().removeConference(conference_id);
}

bool
addParticipant(const std::string& callID, const std::string& confID)
{
    return ring::Manager::instance().addParticipant(callID, confID);
}

bool
addMainParticipant(const std::string& confID)
{
    return ring::Manager::instance().addMainParticipant(confID);
}

bool
detachParticipant(const std::string& callID)
{
    return ring::Manager::instance().detachParticipant(callID);
}

bool
joinConference(const std::string& sel_confID, const std::string& drag_confID)
{
    return ring::Manager::instance().joinConference(sel_confID, drag_confID);
}

bool
holdConference(const std::string& confID)
{
    return ring::Manager::instance().holdConference(confID);
}

bool
unholdConference(const std::string& confID)
{
    return ring::Manager::instance().unHoldConference(confID);
}

std::map<std::string, std::string>
getConferenceDetails(const std::string& callID)
{
    return ring::Manager::instance().getConferenceDetails(callID);
}

std::vector<std::string>
getConferenceList()
{
    return ring::Manager::instance().getConferenceList();
}

std::vector<std::string>
getParticipantList(const std::string& confID)
{
    return ring::Manager::instance().getParticipantList(confID);
}

std::vector<std::string>
getDisplayNames(const std::string& confID)
{
    return ring::Manager::instance().getDisplayNames(confID);
}

std::string
getConferenceId(const std::string& callID)
{
    return ring::Manager::instance().getConferenceId(callID);
}

bool
startRecordedFilePlayback(const std::string& filepath)
{
    return ring::Manager::instance().startRecordedFilePlayback(filepath);
}

void
stopRecordedFilePlayback(const std::string& filepath)
{
   ring::Manager::instance().stopRecordedFilePlayback(filepath);
}

bool
toggleRecording(const std::string& callID)
{
    return ring::Manager::instance().toggleRecordingCall(callID);
}

void
setRecording(const std::string& callID)
{
    toggleRecording(callID);
}

void
recordPlaybackSeek(double value)
{
   ring::Manager::instance().recordingPlaybackSeek(value);
}

bool
getIsRecording(const std::string& callID)
{
    return ring::Manager::instance().isRecording(callID);
}

std::string
getCurrentAudioCodecName(const std::string&)
{
    RING_WARN("Deprecated");
    return "";
}

std::map<std::string, std::string>
getCallDetails(const std::string& callID)
{
    return ring::Manager::instance().getCallDetails(callID);
}

std::vector<std::string>
getCallList()
{
    return ring::Manager::instance().getCallList();
}

void
playDTMF(const std::string& key)
{
    auto code = key.data()[0];
    ring::Manager::instance().playDtmf(code);

    if (auto current_call = ring::Manager::instance().getCurrentCall())
        current_call->carryingDTMFdigits(code);
}

void
startTone(int32_t start, int32_t type)
{
    if (start) {
        if (type == 0)
           ring::Manager::instance().playTone();
        else
           ring::Manager::instance().playToneWithMessage();
    } else
       ring::Manager::instance().stopTone();
}

bool
switchInput(const std::string& callID, const std::string& resource)
{
    return ring::Manager::instance().switchInput(callID, resource);
}

void
setSASVerified(const std::string& /*callID*/)
{
    RING_ERR("ZRTP not supported");
}

void
resetSASVerified(const std::string& /*callID*/)
{
    RING_ERR("ZRTP not supported");
}

void
setConfirmGoClear(const std::string& /*callID*/)
{
    RING_ERR("ZRTP not supported");
}

void
requestGoClear(const std::string& /*callID*/)
{
    RING_ERR("ZRTP not supported");
}

void
acceptEnrollment(const std::string& /*callID*/, bool /*accepted*/)
{
    RING_ERR("ZRTP not supported");
}

void
sendTextMessage(const std::string& callID, const std::string& message, const std::string& from)
{
#if HAVE_INSTANT_MESSAGING
   ring::Manager::instance().sendTextMessage(callID, message, from);
#endif
}

void
sendTextMessage(const std::string& callID, const std::string& message)
{
#if HAVE_INSTANT_MESSAGING
    ring::Manager::instance().sendTextMessage(callID, message, "Me");
#else
    RING_ERR("Could not send \"%s\" text message to %s since Ring daemon does not support it, please recompile with instant messaging support", message.c_str(), callID.c_str());
#endif
}

} // namespace DRing
