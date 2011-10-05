/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#include "iaxvoiplink.h"
#include "iaxcall.h"
#include "eventthread.h"
#include "im/InstantMessaging.h"
#include "iaxaccount.h"
#include "manager.h"
#include "hooks/urlhook.h"
#include "audio/audiolayer.h"
#include "audio/samplerateconverter.h"

#include <cmath>
#include <dlfcn.h>

IAXVoIPLink::IAXVoIPLink (const std::string& accountID) :
    evThread_(new EventThread(this))
    , regSession_(NULL)
    , nextRefreshStamp_(0)
    , converter_(44100)
    , initDone_(false)
	, accountID_(accountID)
{
    srand(time(NULL));    // to get random number for RANDOM_PORT
}


IAXVoIPLink::~IAXVoIPLink()
{
	delete evThread_;

    regSession_ = NULL; // shall not delete it // XXX: but why?
    terminate();
}

void
IAXVoIPLink::init()
{
    if (initDone_)
        return;

    for (int port = IAX_DEFAULT_PORTNO, nbTry = 0; nbTry < 3 ; port = rand() % 64000 + 1024, nbTry++) {
        if (iax_init(port) >= 0) {
            evThread_->start();
        	initDone_ = true;
        	break;
        }
    }
}

void
IAXVoIPLink::terminate()
{
    if (!initDone_)
        return;

    ost::MutexLock m(_callMapMutex);
    for (CallMap::iterator iter = _callMap.begin(); iter != _callMap.end(); ++iter) {
        IAXCall *call = dynamic_cast<IAXCall*> (iter->second);
        if (call) {
			ost::MutexLock lock(mutexIAX_);
			iax_hangup (call->session, const_cast<char*>("Dumped Call"));
            delete call;
        }
    }

    _callMap.clear();

    initDone_ = false;
}

void
IAXVoIPLink::getEvent()
{
	mutexIAX_.enter();
	iax_event *event;
	while ((event = iax_get_event(0)) != NULL) {
		// If we received an 'ACK', libiax2 tells apps to ignore them.
		if (event->etype == IAX_EVENT_NULL) {
			iax_event_free (event);
			continue;
		}

		IAXCall *call = iaxFindCallBySession (event->session);

		if (call)
			iaxHandleCallEvent (event, call);
		else if (event->session && event->session == regSession_)
			iaxHandleRegReply (event);  // This is a registration session, deal with it
		else // We've got an event before it's associated with any call
			iaxHandlePrecallEvent (event);

		iax_event_free (event);
	}
	mutexIAX_.leave();

	if (nextRefreshStamp_ && nextRefreshStamp_ < time (NULL))
    	sendRegister(dynamic_cast<IAXAccount *> (Manager::instance().getAccount(accountID_)));

    sendAudioFromMic();

    // thread wait 3 millisecond
    evThread_->sleep(3);
}

void
IAXVoIPLink::sendAudioFromMic (void)
{
    for (CallMap::const_iterator iter = _callMap.begin(); iter != _callMap.end() ; ++iter) {
        IAXCall *currentCall = dynamic_cast<IAXCall*>(iter->second);
        if (!currentCall or currentCall->getState() != Call::Active)
			continue;

		int codecType = currentCall->getAudioCodec();
	    sfl::AudioCodec *audioCodec = static_cast<sfl::AudioCodec *>(Manager::instance().audioCodecFactory.getCodec(codecType));

		if (!audioCodec)
			continue;

		Manager::instance().getMainBuffer()->setInternalSamplingRate (audioCodec->getClockRate());

		unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();

		// we have to get 20ms of data from the mic *20/1000 = /50
		// rate/50 shall be lower than IAX__20S_48KHZ_MAX
		int bytesNeeded = mainBufferSampleRate * 20 / 1000 * sizeof (SFLDataFormat);
		if (Manager::instance().getMainBuffer()->availForGet (currentCall->getCallId()) < bytesNeeded)
			continue;

		// Get bytes from micRingBuffer to data_from_mic
        int bytes = Manager::instance().getMainBuffer()->getData (decData, bytesNeeded, currentCall->getCallId());
        int samples = bytes / sizeof(SFLDataFormat);

		int compSize;
		unsigned int audioRate = audioCodec->getClockRate();
		int outSamples;
		SFLDataFormat *in;
		if (audioRate != mainBufferSampleRate) {
			converter_.resample (decData, resampledData, audioRate, mainBufferSampleRate, samples);
			in = resampledData;
            outSamples = 0;
		} else {
			outSamples = samples;
			in = decData;
		}
		compSize = audioCodec->encode (encodedData, in, DEC_BUFFER_SIZE);

		if (currentCall->session and bytes > 0) {
			ost::MutexLock m(mutexIAX_);
			if (iax_send_voice (currentCall->session, currentCall->format, encodedData, compSize, outSamples) == -1)
				_error ("IAX: Error sending voice data.");
		}
    }
}


IAXCall*
IAXVoIPLink::getIAXCall (const std::string& id)
{
    return dynamic_cast<IAXCall*>(getCall(id));
}

void
IAXVoIPLink::sendRegister (Account *a)
{
    IAXAccount *account = dynamic_cast<IAXAccount*>(a);

    if (account->getHostname().empty())
    	throw VoipLinkException("Account hostname is empty");

    if (account->getUsername().empty())
    	throw VoipLinkException("Account username is empty");

    ost::MutexLock m(mutexIAX_);

    if (regSession_)
        iax_destroy(regSession_);

    regSession_ = iax_session_new();

    if (regSession_) {
        iax_register (regSession_, account->getHostname().data(), account->getUsername().data(), account->getPassword().data(), 120);
        nextRefreshStamp_ = time (NULL) + 10;
        account->setRegistrationState(Trying);
    }
}

void
IAXVoIPLink::sendUnregister (Account *a)
{
	if (regSession_) {
		ost::MutexLock m(mutexIAX_);
		iax_destroy (regSession_);
		regSession_ = NULL;
	}

    nextRefreshStamp_ = 0;

    dynamic_cast<IAXAccount*>(a)->setRegistrationState(Unregistered);
}

Call*
IAXVoIPLink::newOutgoingCall (const std::string& id, const std::string& toUrl)
{
    IAXCall* call = new IAXCall (id, Call::Outgoing);

    call->setPeerNumber (toUrl);
    call->initRecFileName (toUrl);

    iaxOutgoingInvite (call);
	call->setConnectionState (Call::Progressing);
	call->setState (Call::Active);
	addCall (call);

    return call;
}


void
IAXVoIPLink::answer (Call *c)
{
    IAXCall* call = dynamic_cast<IAXCall*>(c);

    Manager::instance().addStream (call->getCallId());

	mutexIAX_.enter();
	iax_answer(call->session);
	mutexIAX_.leave();

    call->setState(Call::Active);
    call->setConnectionState(Call::Connected);

    Manager::instance().getMainBuffer()->flushAllBuffers();
}

void
IAXVoIPLink::hangup (const std::string& id)
{
    _debug ("IAXVoIPLink: Hangup");

    IAXCall* call = getIAXCall(id);
    if (call == NULL)
    	throw VoipLinkException("Could not find call");

    Manager::instance().getMainBuffer()->unBindAll(call->getCallId());

    mutexIAX_.enter();
    iax_hangup (call->session, (char*) "Dumped Call");
    mutexIAX_.leave();

    call->session = NULL;

    removeCall(id);
}


void
IAXVoIPLink::peerHungup (const std::string& id)
{
    _debug ("IAXVoIPLink: Peer hung up");

    IAXCall* call = getIAXCall (id);
    if (call == NULL)
    	throw VoipLinkException("Could not find call");

    Manager::instance().getMainBuffer()->unBindAll (call->getCallId());

    call->session = NULL;

    removeCall (id);
}



void
IAXVoIPLink::onhold (const std::string& id)
{
    IAXCall* call = getIAXCall (id);
    if (call == NULL)
    	throw VoipLinkException("Call does not exist");

    Manager::instance().getMainBuffer()->unBindAll (call->getCallId());

    mutexIAX_.enter();
    iax_quelch_moh (call->session, true);
    mutexIAX_.leave();

    call->setState (Call::Hold);
}

void
IAXVoIPLink::offhold (const std::string& id)
{
    IAXCall* call = getIAXCall (id);
    if (call == NULL)
    	throw VoipLinkException("Call does not exist");

    Manager::instance().addStream (call->getCallId());

    mutexIAX_.enter();
    iax_unquelch (call->session);
    mutexIAX_.leave();
    Manager::instance().getAudioDriver()->startStream();
    call->setState (Call::Active);
}

void
IAXVoIPLink::transfer (const std::string& id, const std::string& to)
{
    IAXCall* call = getIAXCall (id);
    if (!call)
    	return;

    char callto[to.length() +1];
    strcpy (callto, to.c_str());

    mutexIAX_.enter();
    iax_transfer (call->session, callto);
    mutexIAX_.leave();
}

bool
IAXVoIPLink::attendedTransfer(const std::string& /*transferID*/, const std::string& /*targetID*/)
{
	return false; // TODO
}

void
IAXVoIPLink::refuse (const std::string& id)
{
    IAXCall* call = getIAXCall (id);
    if (call) {
		mutexIAX_.enter();
		iax_reject (call->session, (char*) "Call rejected manually.");
		mutexIAX_.leave();

		removeCall (id);
    }
}


void
IAXVoIPLink::carryingDTMFdigits (const std::string& id, char code)
{
    IAXCall* call = getIAXCall (id);
    if (call) {
		mutexIAX_.enter();
		iax_send_dtmf (call->session, code);
		mutexIAX_.leave();
    }
}

void
IAXVoIPLink::sendTextMessage (sfl::InstantMessaging *module,
        const std::string& callID, const std::string& message,
        const std::string& /*from*/)
{
    IAXCall* call = getIAXCall (callID);
    if (call) {
		mutexIAX_.enter();
		module->send_iax_message (call->session, callID, message.c_str());
		mutexIAX_.leave();
    }
}


std::string
IAXVoIPLink::getCurrentCodecName(Call *c) const
{
    IAXCall *call = dynamic_cast<IAXCall*>(c);
    sfl::Codec *audioCodec = Manager::instance().audioCodecFactory.getCodec(call->getAudioCodec());
    return audioCodec ? audioCodec->getMimeSubtype() : "";
}


void
IAXVoIPLink::iaxOutgoingInvite (IAXCall* call)
{
    ost::MutexLock m(mutexIAX_);

    call->session = iax_session_new();

    IAXAccount *account = dynamic_cast<IAXAccount *>(Manager::instance().getAccount(accountID_));
    std::string username(account->getUsername());
    std::string strNum(username + ":" + account->getPassword() + "@" + account->getHostname() + "/" + call->getPeerNumber());

    /** @todo Make preference dynamic, and configurable */
    int audio_format_preferred = call->getFirstMatchingFormat(call->getSupportedFormat(accountID_), accountID_);
    int audio_format_capability = call->getSupportedFormat(accountID_);

    iax_call(call->session, username.c_str(), username.c_str(), strNum.c_str(),
              NULL, 0, audio_format_preferred, audio_format_capability);
}


IAXCall*
IAXVoIPLink::iaxFindCallBySession (struct iax_session* session)
{
    ost::MutexLock m(_callMapMutex);
    for (CallMap::const_iterator iter = _callMap.begin(); iter != _callMap.end(); ++iter) {
        IAXCall* call = dynamic_cast<IAXCall*> (iter->second);
        if (call and call->session == session)
            return call;
    }

    return NULL;
}

void
IAXVoIPLink::iaxHandleCallEvent (iax_event* event, IAXCall* call)
{
    std::string id = call->getCallId();

    switch (event->etype) {
        case IAX_EVENT_HANGUP:
            Manager::instance().peerHungupCall (id);

            removeCall (id);
            break;

        case IAX_EVENT_REJECT:
            call->setConnectionState (Call::Connected);
            call->setState (Call::Error);
            Manager::instance().callFailure (id);
            removeCall (id);
            break;

        case IAX_EVENT_ACCEPT:
            if (event->ies.format)
                call->format = event->ies.format;
            break;

        case IAX_EVENT_ANSWER:
        case IAX_EVENT_TRANSFER:
            if (call->getConnectionState() == Call::Connected)
                break;

            Manager::instance().addStream (call->getCallId());

            call->setConnectionState (Call::Connected);
            call->setState (Call::Active);
            if (event->ies.format)
                call->format = event->ies.format;

            Manager::instance().peerAnsweredCall (id);

            Manager::instance().getAudioDriver()->startStream();
            Manager::instance().getMainBuffer()->flushAllBuffers();

            break;

        case IAX_EVENT_BUSY:
            call->setConnectionState (Call::Connected);
            call->setState (Call::Busy);
            Manager::instance().callBusy (id);
            removeCall (id);
            break;

        case IAX_EVENT_VOICE:
            iaxHandleVoiceEvent (event, call);
            break;

        case IAX_EVENT_TEXT:
            Manager::instance ().incomingMessage (call->getCallId (), call->getPeerNumber(), std::string ( (const char*) event->data));
            break;

        case IAX_EVENT_RINGA:
            call->setConnectionState (Call::Ringing);
            Manager::instance().peerRingingCall (call->getCallId());
            break;

        case IAX_IE_MSGCOUNT:
        case IAX_EVENT_TIMEOUT:
        case IAX_EVENT_PONG:
        default:
            break;

        case IAX_EVENT_URL:
            if (Manager::instance().getConfigString ("Hooks", "Hooks.iax2_enabled") == "1")
				UrlHook::runAction (Manager::instance().getConfigString ("Hooks", "Hooks.url_command"), (char*) event->data);
            break;
    }
}


/* Handle audio event, VOICE packet received */
void
IAXVoIPLink::iaxHandleVoiceEvent (iax_event* event, IAXCall* call)
{
    // Skip this empty packet.
    if (!event->datalen)
        return;

    sfl::AudioCodec *audioCodec = static_cast<sfl::AudioCodec *>(Manager::instance().audioCodecFactory.getCodec(call->getAudioCodec()));
    if (!audioCodec)
        return;

	Manager::instance().getMainBuffer()->setInternalSamplingRate(audioCodec->getClockRate());
	unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();

	if (event->subclass)
		call->format = event->subclass;

	unsigned char *data = (unsigned char*) event->data;
	unsigned int size = event->datalen;

	unsigned int max = audioCodec->getClockRate() * 20 / 1000;

	if (size > max)
		size = max;

	int samples = audioCodec->decode (decData, data , size);
	int outSize = samples * sizeof(SFLDataFormat);
	SFLDataFormat *out = decData;
	unsigned int audioRate = audioCodec->getClockRate();
	if (audioRate != mainBufferSampleRate) {
		outSize = (double)outSize * (mainBufferSampleRate / audioRate);
		converter_.resample (decData, resampledData, mainBufferSampleRate, audioRate, samples);
		out = resampledData;
	}
	Manager::instance().getMainBuffer()->putData (out, outSize, call->getCallId());
}

/**
 * Handle the registration process
 */
void
IAXVoIPLink::iaxHandleRegReply (iax_event* event)
{
    IAXAccount *account = dynamic_cast<IAXAccount *>(Manager::instance().getAccount(accountID_));
    if (event->etype != IAX_EVENT_REGREJ && event->etype != IAX_EVENT_REGACK)
		return;

	ost::MutexLock m(mutexIAX_);
	iax_destroy(regSession_);
	regSession_ = NULL;

	account->setRegistrationState((event->etype == IAX_EVENT_REGREJ) ? ErrorAuth : Registered);

    if (event->etype == IAX_EVENT_REGACK)
        nextRefreshStamp_ = time (NULL) + (event->ies.refresh ? event->ies.refresh : 60);
}

void
IAXVoIPLink::iaxHandlePrecallEvent (iax_event* event)
{
    IAXCall *call;
    std::string id;
    int format;

    switch (event->etype) {
        case IAX_EVENT_CONNECT:
            id = Manager::instance().getNewCallID();

            call = new IAXCall (id, Call::Incoming);
            call->session = event->session;
            call->setConnectionState (Call::Progressing);

            if (event->ies.calling_number)
                call->setPeerNumber(event->ies.calling_number);

            if (event->ies.calling_name)
                call->setPeerName (std::string (event->ies.calling_name));

            // if peerNumber exist append it to the name string
            call->initRecFileName (std::string (event->ies.calling_number));
            Manager::instance().incomingCall(call, accountID_);

            format = call->getFirstMatchingFormat (event->ies.format, accountID_);
            if (!format)
				format = call->getFirstMatchingFormat (event->ies.capability, accountID_);

			iax_accept (event->session, format);
			iax_ring_announce (event->session);
			addCall (call);
			call->format = format;

            break;

        case IAX_EVENT_HANGUP:
            id = iaxFindCallBySession(event->session)->getCallId();
            Manager::instance().peerHungupCall (id);
            removeCall (id);
            break;

        case IAX_EVENT_TIMEOUT: // timeout for an unknown session
        case IAX_IE_MSGCOUNT:
        case IAX_EVENT_REGACK:
        case IAX_EVENT_REGREJ:
        case IAX_EVENT_REGREQ:
            // Received when someone wants to register to us!?!
            // Asterisk receives and answers to that, not us, we're a phone.
        default:
            break;
    }
}
