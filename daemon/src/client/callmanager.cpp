/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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

#include "callmanager.h"
#include "call_factory.h"

#include "sip/sipcall.h"
#include "sip/sipvoiplink.h"
#include "audio/audiolayer.h"

#include "logger.h"
#include "manager.h"

namespace DRing {

using ring::callManager;

void registerEvHandlers(struct call_ev_handlers* evHandlers)
{
    callManager.evHandlers_ = *evHandlers;
}

bool placeCall(const std::string& accountID,
                            const std::string& callID,
                            const std::string& to)
{
    // Check if a destination number is available
    if (to.empty()) {
        RING_DBG("No number entered - Call stopped");
        return false;
    } else {
        return::ring::Manager::instance().outgoingCall(accountID, callID, to);
    }
}

bool
refuse(const std::string& callID)
{
    return::ring::Manager::instance().refuseCall(callID);
}

bool
accept(const std::string& callID)
{
    return::ring::Manager::instance().answerCall(callID);
}

bool
hangUp(const std::string& callID)
{
    return::ring::Manager::instance().hangupCall(callID);
}

bool
hangUpConference(const std::string& confID)
{
    return::ring::Manager::instance().hangupConference(confID);
}

bool
hold(const std::string& callID)
{
    return::ring::Manager::instance().onHoldCall(callID);
}

bool
unhold(const std::string& callID)
{
    return::ring::Manager::instance().offHoldCall(callID);
}

bool
transfer(const std::string& callID, const std::string& to)
{
    return::ring::Manager::instance().transferCall(callID, to);
}

bool
attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    return::ring::Manager::instance().attendedTransfer(transferID, targetID);
}

bool
joinParticipant(const std::string& sel_callID,
                             const std::string& drag_callID)
{
    return::ring::Manager::instance().joinParticipant(sel_callID, drag_callID);
}

void
createConfFromParticipantList(const std::vector<std::string>& participants)
{
   ::ring::Manager::instance().createConfFromParticipantList(participants);
}

bool
isConferenceParticipant(const std::string& callID)
{
    return ::ring::Manager::instance().isConferenceParticipant(callID);
}

void
removeConference(const std::string& conference_id)
{
   ::ring::Manager::instance().removeConference(conference_id);
}

bool
addParticipant(const std::string& callID, const std::string& confID)
{
    return ::ring::Manager::instance().addParticipant(callID, confID);
}

bool
addMainParticipant(const std::string& confID)
{
    return::ring::Manager::instance().addMainParticipant(confID);
}

bool
detachParticipant(const std::string& callID)
{
    return::ring::Manager::instance().detachParticipant(callID);
}

bool
joinConference(const std::string& sel_confID, const std::string& drag_confID)
{
    return::ring::Manager::instance().joinConference(sel_confID, drag_confID);
}

bool
holdConference(const std::string& confID)
{
    return::ring::Manager::instance().holdConference(confID);
}

bool
unholdConference(const std::string& confID)
{
    return::ring::Manager::instance().unHoldConference(confID);
}

std::map<std::string, std::string>
getConferenceDetails(const std::string& callID)
{
    return::ring::Manager::instance().getConferenceDetails(callID);
}

std::vector<std::string>
getConferenceList()
{
    return::ring::Manager::instance().getConferenceList();
}

std::vector<std::string>
getParticipantList(const std::string& confID)
{
    return::ring::Manager::instance().getParticipantList(confID);
}

std::vector<std::string>
getDisplayNames(const std::string& confID)
{
    return::ring::Manager::instance().getDisplayNames(confID);
}

std::string
getConferenceId(const std::string& callID)
{
    return::ring::Manager::instance().getConferenceId(callID);
}

bool
startRecordedFilePlayback(const std::string& filepath)
{
    return::ring::Manager::instance().startRecordedFilePlayback(filepath);
}

void
stopRecordedFilePlayback(const std::string& filepath)
{
   ::ring::Manager::instance().stopRecordedFilePlayback(filepath);
}

bool
toggleRecording(const std::string& callID)
{
    return::ring::Manager::instance().toggleRecordingCall(callID);
}

void
setRecording(const std::string& callID)
{
    toggleRecording(callID);
}

void
recordPlaybackSeek(double value)
{
   ::ring::Manager::instance().recordingPlaybackSeek(value);
}

bool
getIsRecording(const std::string& callID)
{
    return::ring::Manager::instance().isRecording(callID);
}

std::string getCurrentAudioCodecName(const std::string& /*callID*/)
{
    RING_WARN("Deprecated");
    return "";
}

std::map<std::string, std::string>
getCallDetails(const std::string& callID)
{
    return::ring::Manager::instance().getCallDetails(callID);
}

std::vector<std::string>
getCallList()
{
    return::ring::Manager::instance().getCallList();
}

void
playDTMF(const std::string& key)
{
    auto code = key.data()[0];
   ::ring::Manager::instance().playDtmf(code);

    if (auto current_call =::ring::Manager::instance().getCurrentCall())
        current_call->carryingDTMFdigits(code);
}

void
startTone(int32_t start, int32_t type)
{
    if (start) {
        if (type == 0)
           ::ring::Manager::instance().playTone();
        else
           ::ring::Manager::instance().playToneWithMessage();
    } else
       ::ring::Manager::instance().stopTone();
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

void sendTextMessage(const std::string& callID, const std::string& message, const std::string& from)
{
#if HAVE_INSTANT_MESSAGING
   ::ring::Manager::instance().sendTextMessage(callID, message, from);
#endif
}

void
sendTextMessage(const std::string& callID, const std::string& message)
{
#if HAVE_INSTANT_MESSAGING
    if (!::ring::Manager::instance().sendTextMessage(callID, message, "Me"))
        throw CallManagerException();
#else
    RING_ERR("Could not send \"%s\" text message to %s since SFLphone daemon does not support it, please recompile with instant messaging support", message.c_str(), callID.c_str());
#endif
}

} // namespace DRing

namespace ring {

CallManager callManager;

void callStateChanged(const std::string& callID, const std::string& state)
{
    if (callManager.evHandlers_.on_state_change) {
        callManager.evHandlers_.on_state_change(callID, state);
    }
}

void transferFailed()
{
    if (callManager.evHandlers_.on_transfer_fail) {
        callManager.evHandlers_.on_transfer_fail();
    }
}

void transferSucceeded()
{
    if (callManager.evHandlers_.on_transfer_success) {
        callManager.evHandlers_.on_transfer_success();
    }
}

void recordPlaybackStopped(const std::string& path)
{
    if (callManager.evHandlers_.on_record_playback_stopped) {
        callManager.evHandlers_.on_record_playback_stopped(path);
    }
}

void voiceMailNotify(const std::string& callID, int32_t nd_msg)
{
    if (callManager.evHandlers_.on_voice_mail_notify) {
        callManager.evHandlers_.on_voice_mail_notify(callID, nd_msg);
    }
}

void onIncomingMessage(const std::string& ID, const std::string& from, const std::string& msg)
{
    if (callManager.evHandlers_.on_incoming_message) {
        callManager.evHandlers_.on_incoming_message(ID, from, msg);
    }
}

void onIncomingCall(const std::string& accountID, const std::string& callID, const std::string& from)
{
    if (callManager.evHandlers_.on_incoming_call) {
        callManager.evHandlers_.on_incoming_call(accountID, callID, from);
    }
}

void recordPlaybackFilepath(const std::string& id, const std::string& filename)
{
    if (callManager.evHandlers_.on_record_playback_filepath) {
        callManager.evHandlers_.on_record_playback_filepath(id, filename);
    }
}

void conferenceCreated(const std::string& confID)
{
    if (callManager.evHandlers_.on_conference_created) {
        callManager.evHandlers_.on_conference_created(confID);
    }
}

void conferenceChanged(const std::string& confID, const std::string& state)
{
    if (callManager.evHandlers_.on_conference_changed) {
        callManager.evHandlers_.on_conference_changed(confID, state);
    }
}

void updatePlaybackScale(const std::string& filepath, int32_t position, int32_t scale)
{
    if (callManager.evHandlers_.on_update_playback_scale) {
        callManager.evHandlers_.on_update_playback_scale(filepath, position, scale);
    }
}

void conferenceRemoved(const std::string& confID)
{
    if (callManager.evHandlers_.on_conference_remove) {
        callManager.evHandlers_.on_conference_remove(confID);
    }
}

void newCallCreated(const std::string& accountID, const std::string& callID, const std::string& to)
{
    if (callManager.evHandlers_.on_new_call) {
        callManager.evHandlers_.on_new_call(accountID, callID, to);
    }
}

void sipCallStateChanged(const std::string& callID, const std::string& state, int32_t code)
{
    if (callManager.evHandlers_.on_sip_call_state_change) {
        callManager.evHandlers_.on_sip_call_state_change(callID, state, code);
    }
}

void recordingStateChanged(const std::string& callID, bool state)
{
    if (callManager.evHandlers_.on_record_state_change) {
        callManager.evHandlers_.on_record_state_change(callID, state);
    }
}

void secureSdesOn(const std::string& callID)
{
    if (callManager.evHandlers_.on_secure_sdes_on) {
        callManager.evHandlers_.on_secure_sdes_on(callID);
    }
}

void secureSdesOff(const std::string& callID)
{
    if (callManager.evHandlers_.on_secure_sdes_off) {
        callManager.evHandlers_.on_secure_sdes_off(callID);
    }
}

void secureZrtpOn(const std::string& callID, const std::string& cipher)
{
    if (callManager.evHandlers_.on_secure_zrtp_on) {
        callManager.evHandlers_.on_secure_zrtp_on(callID, cipher);
    }
}

void secureZrtpOff(const std::string& callID)
{
    if (callManager.evHandlers_.on_secure_zrtp_off) {
        callManager.evHandlers_.on_secure_zrtp_off(callID);
    }
}

void showSAS(const std::string& callID, const std::string& sas, bool verified)
{
    if (callManager.evHandlers_.on_show_sas) {
        callManager.evHandlers_.on_show_sas(callID, sas, verified);
    }
}

void zrtpNotSuppOther(const std::string& callID)
{
    if (callManager.evHandlers_.on_zrtp_not_supp_other) {
        callManager.evHandlers_.on_zrtp_not_supp_other(callID);
    }
}

void zrtpNegotiationFailed(const std::string& callID, const std::string& reason, const std::string& severity)
{
    if (callManager.evHandlers_.on_zrtp_negotiation_fail) {
        callManager.evHandlers_.on_zrtp_negotiation_fail(callID, reason, severity);
    }
}

void onRtcpReportReceived(const std::string& callID, const std::map<std::string, int>& stats)
{
    if (callManager.evHandlers_.on_rtcp_receive_report) {
        callManager.evHandlers_.on_rtcp_receive_report(callID, stats);
    }
}

} // namespace ring
