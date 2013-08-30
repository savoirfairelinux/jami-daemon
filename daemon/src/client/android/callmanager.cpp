/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Emeric Vigier <emeric.vigier@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "global.h"
#include "client/callmanager.h"
#include "jni_callbacks.h"

#include "sip/sipcall.h"
#include "sip/sipvoiplink.h"
#include "audio/audiolayer.h"
#include "audio/audiortp/audio_rtp_factory.h"
#if HAVE_ZRTP
#include "audio/audiortp/audio_zrtp_session.h"
#endif

#include "logger.h"
#include "manager.h"

CallManager::CallManager()
{}

bool CallManager::placeCall(const std::string& accountID,
                            const std::string& callID,
                            const std::string& to)
{
    // Check if a destination number is available
    if (to.empty()) {
        DEBUG("No number entered - Call stopped");
        return false;
    } else {
        return Manager::instance().outgoingCall(accountID, callID, to);
    }
}

bool
CallManager::refuse(const std::string& callID)
{
    return Manager::instance().refuseCall(callID);
}

bool
CallManager::accept(const std::string& callID)
{
    return Manager::instance().answerCall(callID);
}

bool
CallManager::hangUp(const std::string& callID)
{
    return Manager::instance().hangupCall(callID);
}

bool
CallManager::hangUpConference(const std::string& confID)
{
    return Manager::instance().hangupConference(confID);
}

bool
CallManager::hold(const std::string& callID)
{
    return Manager::instance().onHoldCall(callID);
}

bool
CallManager::unhold(const std::string& callID)
{
    return Manager::instance().offHoldCall(callID);
}

bool
CallManager::transfer(const std::string& callID, const std::string& to)
{
    return Manager::instance().transferCall(callID, to);
}

bool CallManager::attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    return Manager::instance().attendedTransfer(transferID, targetID);
}

void CallManager::setVolume(const std::string& device, const double& value)
{
    AudioLayer *audiolayer = Manager::instance().getAudioDriver();

    if(!audiolayer) {
        ERROR("Audio layer not valid while updating volume");
        return;
    }

    DEBUG("DBUS set volume for %s: %f", device.c_str(), value);

    if (device == "speaker") {
        audiolayer->setPlaybackGain((int)(value * 100.0));
    } else if (device == "mic") {
        audiolayer->setCaptureGain((int)(value * 100.0));
    }

    //volumeChanged(device, value);
}

double
CallManager::getVolume(const std::string& device)
{
    AudioLayer *audiolayer = Manager::instance().getAudioDriver();

    if(!audiolayer) {
        ERROR("Audio layer not valid while updating volume");
        return 0.0;
    }

    if (device == "speaker")
        return audiolayer->getPlaybackGain() / 100.0;
    else if (device == "mic")
        return audiolayer->getCaptureGain() / 100.0;

    return 0;
}

void
CallManager::sendTextMessage(const std::string& callID, const std::string& message, const std::string& from)
{
#if HAVE_INSTANT_MESSAGING
    Manager::instance().sendTextMessage(callID, message, from);
#endif
}

void
CallManager::sendTextMessage(const std::string& callID, const std::string& message)
{
#if HAVE_INSTANT_MESSAGING
    try{
        Manager::instance().sendTextMessage(callID, message, "Me");
    }catch(...){
        ERROR("Could not send \"%s\" text message to %s", message.c_str(), callID.c_str());
    }

#else
    ERROR("Could not send \"%s\" text message to %s since SFLphone daemon does not support it, please recompile with instant messaging support", message.c_str(), callID.c_str());
#endif
}


bool CallManager::toggleRecording(const std::string& id)
{
    return Manager::instance().toggleRecordingCall(id);
}

bool
CallManager::joinParticipant(const std::string& sel_callID, const std::string& drag_callID)
{
    return Manager::instance().joinParticipant(sel_callID, drag_callID);
}

void
CallManager::createConfFromParticipantList(const std::vector<std::string>& participants)
{
    Manager::instance().createConfFromParticipantList(participants);
}

void
CallManager::removeConference(const std::string& conference_id)
{
    Manager::instance().removeConference(conference_id);
}

bool
CallManager::addParticipant(const std::string& callID, const std::string& confID)
{
    return Manager::instance().addParticipant(callID, confID);
}

bool
CallManager::addMainParticipant(const std::string& confID)
{
    return Manager::instance().addMainParticipant(confID);
}

bool
CallManager::detachParticipant(const std::string& callID)
{
    return Manager::instance().detachParticipant(callID);
}

bool
CallManager::joinConference(const std::string& sel_confID, const std::string& drag_confID)
{
    return Manager::instance().joinConference(sel_confID, drag_confID);
}

bool
CallManager::holdConference(const std::string& confID)
{
    return Manager::instance().holdConference(confID);
}

bool
CallManager::unholdConference(const std::string& confID)
{
    return Manager::instance().unHoldConference(confID);
}

bool
CallManager::isConferenceParticipant(const std::string& call_id)
{
    return Manager::instance().isConferenceParticipant(call_id);
}

std::map<std::string, std::string>
CallManager::getConferenceDetails(const std::string& callID)
{
    return Manager::instance().getConferenceDetails(callID);
}

std::vector<std::string>
CallManager::getConferenceList()
{
    return Manager::instance().getConferenceList();
}

std::vector<std::string>
CallManager::getParticipantList(const std::string& confID)
{
    return Manager::instance().getParticipantList(confID);
}

std::string
CallManager::getConferenceId(const std::string& callID)
{
    return Manager::instance().getConferenceId(callID);
}

bool
CallManager::startRecordedFilePlayback(const std::string& filepath)
{
    return Manager::instance().startRecordedFilePlayback(filepath);
}

void
CallManager::stopRecordedFilePlayback(const std::string& filepath)
{
    Manager::instance().stopRecordedFilePlayback(filepath);
}

void
CallManager::recordPlaybackSeek(const double& value)
{
    Manager::instance().recordingPlaybackSeek(value);
}

bool
CallManager::getIsRecording(const std::string& callID)
{
    return Manager::instance().isRecording(callID);
}

std::string CallManager::getCurrentAudioCodecName(const std::string& callID)
{
    return Manager::instance().getCurrentAudioCodecName(callID);
}

std::map<std::string, std::string>
CallManager::getCallDetails(const std::string& callID)
{
    return Manager::instance().getCallDetails(callID);
}

std::vector<std::string>
CallManager::getCallList()
{
    return Manager::instance().getCallList();
}

void
CallManager::playDTMF(const std::string& key)
{
    Manager::instance().sendDtmf(Manager::instance().getCurrentCallId(), key.data()[0]);
}

void
CallManager::startTone(const int32_t& start , const int32_t& type)
{
    if (start) {
        if (type == 0)
            Manager::instance().playTone();
        else
            Manager::instance().playToneWithMessage();
    } else
        Manager::instance().stopTone();
}

// TODO: this will have to be adapted
// for conferencing in order to get
// the right pointer for the given
// callID.
#if HAVE_ZRTP
sfl::AudioZrtpSession *
CallManager::getAudioZrtpSession(const std::string& callID)
{
    // IP2IP profile is associated with IP2IP profile anyway
    SIPVoIPLink * link = static_cast<SIPVoIPLink *>(Manager::instance().getAccountLink(SIPAccount::IP2IP_PROFILE));

    if (!link)
        throw CallManagerException("Failed to get sip link");

    SIPCall *call;

    try {
        call = link->getSIPCall(callID);
    } catch (const VoipLinkException &e) {
        throw CallManagerException("Call id " + callID + " is not valid");
    }

    sfl::AudioZrtpSession * zSession = call->getAudioRtp().getAudioZrtpSession();

    if (!zSession)
        throw CallManagerException("Failed to get AudioZrtpSession");

    return zSession;
}
#endif

void
CallManager::setSASVerified(const std::string& callID)
{
#if HAVE_ZRTP
    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession(callID);
        zSession->SASVerified();
    } catch (...) {
    }
#else
    ERROR("No zrtp support for %s, please recompile SFLphone with zrtp", callID.c_str());
#endif
}

void
CallManager::resetSASVerified(const std::string& callID)
{
#if HAVE_ZRTP
    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession(callID);
        zSession->resetSASVerified();
    } catch (...) {
    }
#else
    ERROR("No zrtp support for %s, please recompile SFLphone with zrtp", callID.c_str());
#endif
}

void
CallManager::setConfirmGoClear(const std::string& callID)
{
#if HAVE_ZRTP
    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession(callID);
        zSession->goClearOk();
    } catch (...) {
    }
#else
    ERROR("No zrtp support for %s, please recompile SFLphone with zrtp", callID.c_str());
#endif
}

void
CallManager::requestGoClear(const std::string& callID)
{
#if HAVE_ZRTP
    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession(callID);
        zSession->requestGoClear();
    } catch (...) {
    }
#else
    ERROR("No zrtp support for %s, please recompile SFLphone with zrtp", callID.c_str());
#endif
}

void
CallManager::acceptEnrollment(const std::string& callID, const bool& accepted)
{
#if HAVE_ZRTP
    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession(callID);
        zSession->acceptEnrollment(accepted);
    } catch (...) {
    }
#else
    ERROR("No zrtp support for %s, please recompile SFLphone with zrtp", callID.c_str());
#endif
}

void CallManager::callStateChanged(const std::string& callID, const std::string& state)
{
    on_call_state_changed_wrapper(callID, state);
}

void CallManager::transferFailed()
{

}

void CallManager::transferSucceeded()
{

}

void CallManager::recordPlaybackStopped(const std::string& path)
{

}

void CallManager::voiceMailNotify(const std::string& callID, const std::string& nd_msg)
{

}

void CallManager::incomingMessage(const std::string& ID, const std::string& from, const std::string& msg)
{
    on_incoming_message_wrapper(ID, from, msg);
}

void CallManager::incomingCall(const std::string& accountID, const std::string& callID, const std::string& from)
{
    on_incoming_call_wrapper(accountID, callID, from);
}

void CallManager::recordPlaybackFilepath(const std::string& id, const std::string& filename)
{
    on_record_playback_filepath_wrapper(id, filename);
}

void CallManager::conferenceCreated(const std::string& confID)
{
    on_conference_created_wrapper(confID);
}

void CallManager::conferenceChanged(const std::string& confID,const std::string& state)
{
    on_conference_state_changed_wrapper(confID, state);
}

void CallManager::conferenceRemoved(const std::string& confID)
{
    on_conference_removed_wrapper(confID);
}

void CallManager::newCallCreated(const std::string& accountID, const std::string& callID, const std::string& to)
{
    on_new_call_created_wrapper(accountID, callID, to);
}

void CallManager::registrationStateChanged(const std::string& accoundID, const std::string& state, const int32_t& code)
{
    on_account_state_changed_with_code_wrapper(accoundID, state, code);
}

void CallManager::sipCallStateChanged(const std::string& accoundID, const std::string& state, const int32_t& code)
{

}

void CallManager::updatePlaybackScale(const int32_t&, const int32_t&)
{
}
