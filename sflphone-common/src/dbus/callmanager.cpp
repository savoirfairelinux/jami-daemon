
/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <vector>

#include "global.h"
#include "callmanager.h"

#include "sip/sipvoiplink.h"
#include "audio/audiortp/AudioRtpFactory.h"
#include "audio/audiortp/AudioZrtpSession.h"

#include "manager.h"

const char* CallManager::SERVER_PATH = "/org/sflphone/SFLphone/CallManager";

CallManager::CallManager (DBus::Connection& connection)
        : DBus::ObjectAdaptor (connection, SERVER_PATH)
{
}

void
CallManager::placeCall (const std::string& accountID,
                        const std::string& callID,
                        const std::string& to)
{    // Check if a destination number is available

    if (to == "")   _debug ("No number entered - Call stopped");
    else            Manager::instance().outgoingCall (accountID, callID, to);
}

void
CallManager::placeCallFirstAccount (const std::string& callID,
				    const std::string& to)
{

    if (to == "") {
        _warn("No number entered - Call stopped");
	return;
    }

    std::vector< std::string > accountOrder = Manager::instance().loadAccountOrder();
    std::vector< std::string >::iterator iter = accountOrder.begin();

    Account *account;
    while(iter != accountOrder.end()) {
        account = Manager::instance().getAccount(*iter);
	if((*iter != IP2IP_PROFILE) && account->isEnabled()) {
	    Manager::instance().outgoingCall (*iter, callID, to);
	    return;
	}

	iter++;
    }

    _warn("No enabled account found - Call stopped\n");
    
}

void
CallManager::refuse (const std::string& callID)
{
    _debug ("CallManager: refuse received");
    Manager::instance().refuseCall (callID);
}

void
CallManager::accept (const std::string& callID)
{
    _debug ("CallManager: accept received");
    Manager::instance().answerCall (callID);
}

void
CallManager::hangUp (const std::string& callID)
{
    _debug ("CallManager::hangUp received %s", callID.c_str());
    Manager::instance().hangupCall (callID);

}

void
CallManager::hangUpConference (const std::string& confID)
{
    _debug ("CallManager::hangUpConference received %s", confID.c_str());
    Manager::instance().hangupConference (confID);

}


void
CallManager::hold (const std::string& callID)
{
    _debug ("CallManager::hold received %s", callID.c_str());
    Manager::instance().onHoldCall (callID);

}

void
CallManager::unhold (const std::string& callID)
{
    _debug ("CallManager::unhold received %s", callID.c_str());
    Manager::instance().offHoldCall (callID);
}

void
CallManager::transfert (const std::string& callID, const std::string& to)
{
    _debug ("CallManager::transfert received");
    Manager::instance().transferCall (callID, to);
}



void
CallManager::setVolume (const std::string& device, const double& value)
{

    if (device == "speaker") {
        Manager::instance().setSpkrVolume ( (int) (value*100.0));
    } else if (device == "mic") {
        Manager::instance().setMicVolume ( (int) (value*100.0));
    }

    volumeChanged (device, value);
}

double
CallManager::getVolume (const std::string& device)
{

    if (device == "speaker") {
        _debug ("Current speaker = %d", Manager::instance().getSpkrVolume());
        return Manager::instance().getSpkrVolume() /100.0;
    } else if (device == "mic") {
        _debug ("Current mic = %d", Manager::instance().getMicVolume());
        return Manager::instance().getMicVolume() /100.0;
    }

    return 0;
}

void
CallManager::joinParticipant (const std::string& sel_callID, const std::string& drag_callID)
{
    _debug ("CallManager::joinParticipant received %s, %s", sel_callID.c_str(), drag_callID.c_str());
    Manager::instance().joinParticipant (sel_callID, drag_callID);
}

void
CallManager::addParticipant (const std::string& callID, const std::string& confID)
{
    _debug ("CallManager::addParticipant received %s, %s", callID.c_str(), confID.c_str());
    Manager::instance().addParticipant (callID, confID);
}

void
CallManager::addMainParticipant (const std::string& confID)
{
    _debug ("CallManager::addMainParticipant received %s", confID.c_str());
    Manager::instance().addMainParticipant (confID);
}

void
CallManager::detachParticipant (const std::string& callID)
{
    _debug ("CallManager::detachParticipant received %s", callID.c_str());
    Manager::instance().detachParticipant (callID, "");
}

void
CallManager::joinConference (const std::string& sel_confID, const std::string& drag_confID)
{
    _debug ("CallManager::joinConference received %s, %s", sel_confID.c_str(), drag_confID.c_str());
    Manager::instance().joinConference (sel_confID, drag_confID);
}

void
CallManager::holdConference (const std::string& confID)
{
    _debug ("CallManager::holdConference received %s", confID.c_str());
    Manager::instance().holdConference (confID);
}

void
CallManager::unholdConference (const std::string& confID)
{
    _debug ("CallManager::unHoldConference received %s", confID.c_str());
    Manager::instance().unHoldConference (confID);
}

std::map< std::string, std::string >
CallManager::getConferenceDetails (const std::string& callID)
{
    return Manager::instance().getConferenceDetails (callID);
}

std::vector< std::string >
CallManager::getConferenceList (void)
{
    return Manager::instance().getConferenceList();
}

std::vector< std::string >
CallManager::getParticipantList (const std::string& confID)
{
    return Manager::instance().getParticipantList (confID);
}

void
CallManager::setRecording (const std::string& callID)
{
    Manager::instance().setRecordingCall (callID);
}

bool
CallManager::getIsRecording (const std::string& callID)
{
    return Manager::instance().isRecording (callID);
}


std::string
CallManager::getCurrentCodecName (const std::string& callID)
{
    return Manager::instance().getCurrentCodecName (callID).c_str();
}


std::map< std::string, std::string >
CallManager::getCallDetails (const std::string& callID)
{
    return Manager::instance().getCallDetails (callID);
}

std::vector< std::string >
CallManager::getCallList (void)
{
    return Manager::instance().getCallList();
}

std::string
CallManager::getCurrentCallID()
{
    return Manager::instance().getCurrentCallId();
}

void
CallManager::playDTMF (const std::string& key)
{
    Manager::instance().sendDtmf (Manager::instance().getCurrentCallId(), key.c_str() [0]);
}

void
CallManager::startTone (const int32_t& start , const int32_t& type)
{
    if (start == true) {
        if (type == 0)
            Manager::instance().playTone();
        else
            Manager::instance().playToneWithMessage();
    } else
        Manager::instance().stopTone ();
}

// TODO: this will have to be adapted
// for conferencing in order to get
// the right pointer for the given
// callID.
sfl::AudioZrtpSession * CallManager::getAudioZrtpSession (const std::string& callID)
{
    SIPVoIPLink * link = NULL;
    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (AccountNULL));

    if (link == NULL) {
        _debug ("Failed to get sip link");
        throw CallManagerException();
    }

    SIPCall *call = link->getSIPCall (callID);

    sfl::AudioRtpFactory * audioRtp = NULL;
    audioRtp = call->getAudioRtp();

    if (audioRtp == NULL) {
        _debug ("Failed to get AudioRtpFactory");
        throw CallManagerException();
    }

    sfl::AudioZrtpSession * zSession = NULL;

    zSession = audioRtp->getAudioZrtpSession();

    if (zSession == NULL) {
        _debug ("Failed to get AudioZrtpSession");
        throw CallManagerException();
    }

    return zSession;
}

void
CallManager::setSASVerified (const std::string& callID)
{

    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession (callID);
        zSession->SASVerified();
    } catch (...) {
        throw;
    }

}

void
CallManager::resetSASVerified (const std::string& callID)
{

    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession (callID);
        zSession->resetSASVerified();
    } catch (...) {
        throw;
    }

}

void
CallManager::setConfirmGoClear (const std::string& callID)
{
    _debug ("CallManager::setConfirmGoClear received for account %s", callID.c_str());

    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession (callID);
        zSession->goClearOk();
    } catch (...) {
        throw;
    }

}

void
CallManager::requestGoClear (const std::string& callID)
{
    _debug ("CallManager::requestGoClear received for account %s", callID.c_str());

    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession (callID);
        zSession->requestGoClear();
    } catch (...) {
        throw;
    }

}

void
CallManager::acceptEnrollment (const std::string& callID, const bool& accepted)
{

    _debug ("CallManager::acceptEnrollment received for account %s", callID.c_str());

    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession (callID);
        zSession->acceptEnrollment (accepted);
    } catch (...) {
        throw;
    }

}

void
CallManager::setPBXEnrollment (const std::string& callID, const bool& yesNo)
{

    _debug ("CallManager::setPBXEnrollment received for account %s", callID.c_str());

    try {
        sfl::AudioZrtpSession * zSession;
        zSession = getAudioZrtpSession (callID);
        zSession->setPBXEnrollment (yesNo);
    } catch (...) {
        throw;
    }

}
