/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "global.h"
#include "callmanager.h"

#include "sip/sipcall.h"
#include "sip/sipvoiplink.h"
#include "sip/sipaccount.h"
#include "sip/sippresence.h"
#include "audio/audiolayer.h"
#include "audio/audiortp/audio_rtp_factory.h"
#if HAVE_ZRTP
#include "audio/audiortp/audio_zrtp_session.h"
#endif

#include "logger.h"
#include "manager.h"

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

bool
CallManager::attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    return Manager::instance().attendedTransfer(transferID, targetID);
}

bool
CallManager::joinParticipant(const std::string& sel_callID,
                             const std::string& drag_callID)
{
    return Manager::instance().joinParticipant(sel_callID, drag_callID);
}

void
CallManager::createConfFromParticipantList(const std::vector<std::string>& participants)
{
    Manager::instance().createConfFromParticipantList(participants);
}

bool
CallManager::isConferenceParticipant(const std::string& callID)
{
    return  Manager::instance().isConferenceParticipant(callID);
}

void
CallManager::removeConference(const std::string& conference_id)
{
    Manager::instance().removeConference(conference_id);
}

bool
CallManager::addParticipant(const std::string& callID, const std::string& confID)
{
    return  Manager::instance().addParticipant(callID, confID);
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

std::vector<std::string>
CallManager::getDisplayNames(const std::string& confID)
{
    return Manager::instance().getDisplayNames(confID);
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

bool
CallManager::toggleRecording(const std::string& callID)
{
    return Manager::instance().toggleRecordingCall(callID);
}

void
CallManager::setRecording(const std::string& callID)
{
    toggleRecording(callID);
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

std::string CallManager::getCurrentAudioCodecName(const std::string& /*callID*/)
{
    WARN("Deprecated");
    return "";
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

    auto call = link->getSipCall(callID);
    if (!call)
        throw CallManagerException("Call id " + callID + " is not valid");

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

void CallManager::sendTextMessage(const std::string& callID, const std::string& message, const std::string& from)
{
#if HAVE_INSTANT_MESSAGING
    Manager::instance().sendTextMessage(callID, message, from);
#endif
}

void
CallManager::sendTextMessage(const std::string& callID, const std::string& message)
{
#if HAVE_INSTANT_MESSAGING
    if (!Manager::instance().sendTextMessage(callID, message, "Me"))
        throw CallManagerException();
#else
    ERROR("Could not send \"%s\" text message to %s since SFLphone daemon does not support it, please recompile with instant messaging support", message.c_str(), callID.c_str());
#endif
}
