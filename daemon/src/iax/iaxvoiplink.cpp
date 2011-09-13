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

#define IAX_BLOCKING    1
#define IAX_NONBLOCKING 0

#define IAX_SUCCESS  0
#define IAX_FAILURE -1

#define RANDOM_IAX_PORT   rand() % 64000 + 1024

#define MUSIC_ONHOLD true

#define CHK_VALID_CALL   if (call == NULL) { _debug("IAX: Call doesn't exists"); \
	return false; }

namespace {
    const char * const HOOKS = "Hooks"; /** Hooks section */
    const char * const URLHOOK_IAX2_ENABLED = "Hooks.iax2_enabled";
    const char * const URLHOOK_COMMAND = "Hooks.url_command";
} // end anonymous namespace

IAXVoIPLink::IAXVoIPLink (const std::string& accountID) :
    evThread_(new EventThread(this))
    , regSession_(NULL)
    , nextRefreshStamp_(0)
    , audiolayer_(NULL)
    , converterSamplingRate_(0)
    , converter_(new SamplerateConverter (44100))
    , initDone_(false)
	, accountID_(accountID)
{
    // to get random number for RANDOM_PORT
    srand(time(NULL));
}


IAXVoIPLink::~IAXVoIPLink()
{
	delete converter_;
	delete evThread_;

    regSession_ = NULL; // shall not delete it
    terminate();
}

void
IAXVoIPLink::init()
{
    // If it was done, don't do it again, until we call terminate()
    if (initDone_)
        return;

    int port = IAX_DEFAULT_PORTNO;

    int last_port = 0;

    int nbTry = 3;

    while (port != IAX_FAILURE && nbTry) {
        last_port = port;
        port = iax_init (port);

        if (port < 0) {
            _debug ("IAX Warning: already initialize on port %d", last_port);
            port = RANDOM_IAX_PORT;
        } else if (port == IAX_FAILURE) {
            _debug ("IAX Fail to start on port %d", last_port);
            port = RANDOM_IAX_PORT;
        } else {
            _debug ("IAX Info: listening on port %d", last_port);
            evThread_->start();

            audiolayer_ = Manager::instance().getAudioDriver();

            // may be different than one already setted
            converterSamplingRate_ = Manager::instance().getMainBuffer()->getInternalSamplingRate();

            break;
        }

        nbTry--;

        initDone_ = true;
    }

    if (port == IAX_FAILURE or nbTry == 0) {
        _debug ("Fail to initialize iax");
        initDone_ = false;
    }
}

void
IAXVoIPLink::terminate()
{
    // If it was done, don't do it again, until we call init()
    if (!initDone_)
        return;

    // Hangup all calls
    terminateIAXCall();

    initDone_ = false;
}

void
IAXVoIPLink::terminateIAXCall()
{
    ost::MutexLock m(_callMapMutex);
    for (CallMap::iterator iter = _callMap.begin(); iter != _callMap.end(); ++iter) {
        IAXCall *call = dynamic_cast<IAXCall*> (iter->second);
        if (call) {
            {
                ost::MutexLock m(mutexIAX_);
                iax_hangup (call->getSession(), (char*) "Dumped Call");
            }
            call->setSession (NULL);
            delete call;
        }
    }

    _callMap.clear();
}

void
IAXVoIPLink::getEvent()
{
    iax_event* event = NULL;

    {
        ost::MutexLock m(mutexIAX_);
        while ((event = iax_get_event (IAX_NONBLOCKING)) != NULL) {
        // If we received an 'ACK', libiax2 tells apps to ignore them.
            if (event->etype == IAX_EVENT_NULL)
                continue;

            IAXCall *call = iaxFindCallBySession (event->session);

            if (call)
                iaxHandleCallEvent (event, call);
            else if (event->session && event->session == regSession_) {
                // This is a registration session, deal with it
                iaxHandleRegReply (event);
            } else {
                // We've got an event before it's associated with any call
                iaxHandlePrecallEvent (event);
            }

            iax_event_free (event);
        }
    }

    sendAudioFromMic();

    // thread wait 3 millisecond
    evThread_->sleep(3);

    free (event);
}

void
IAXVoIPLink::sendAudioFromMic (void)
{
    // We have to update the audio layer type in case we switched
    // TODO Find out a better way to do it
    updateAudiolayer();

    for (CallMap::const_iterator iter = _callMap.begin(); iter != _callMap.end() ; ++iter) {
        IAXCall *currentCall = dynamic_cast<IAXCall*>(iter->second);
        if (!currentCall or currentCall->getState() != Call::Active)
			continue;

		int codecType = currentCall->getAudioCodec();
	    sfl::AudioCodec *audioCodec = static_cast<sfl::AudioCodec *>(Manager::instance().audioCodecFactory.getCodec(codecType));

		if (!audioCodec or !audiolayer_)
			continue;

		Manager::instance().getMainBuffer()->setInternalSamplingRate (audioCodec->getClockRate());

		unsigned int mainBufferSampleRate = audiolayer_->getMainBuffer()->getInternalSamplingRate();

		// we have to get 20ms of data from the mic *20/1000 = /50
		// rate/50 shall be lower than IAX__20S_48KHZ_MAX
		int bytesNeeded = mainBufferSampleRate * 20 / 1000 * sizeof (SFLDataFormat);
		if (audiolayer_->getMainBuffer()->availForGet (currentCall->getCallId()) < bytesNeeded)
			continue;

		// Get bytes from micRingBuffer to data_from_mic
        int bytes = audiolayer_->getMainBuffer()->getData (decData, bytesNeeded, currentCall->getCallId());
        int samples = bytes / sizeof(SFLDataFormat);

		int compSize;
		unsigned int audioRate = audioCodec->getClockRate();
		int outSamples;
		SFLDataFormat *in;
		if (audioRate != mainBufferSampleRate) {
			converter_->resample (decData, resampledData, audioRate, mainBufferSampleRate, samples);
			in = resampledData;
            outSamples = 0;
		} else {
			outSamples = samples;
			in = decData;
		}
		compSize = audioCodec->encode (encodedData, in, DEC_BUFFER_SIZE);

		// Send it out!
        {
            ost::MutexLock m(mutexIAX_);

            // Make sure the session and the call still exists.
            if (currentCall->getSession() and (bytes > 0)) {
                if (iax_send_voice (currentCall->getSession(), currentCall->getFormat(), encodedData, compSize, outSamples) == -1)
                    _error ("IAX: Error sending voice data.");
            }
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
    _debug ("IAX: Sending registration");

    IAXAccount *account = dynamic_cast<IAXAccount*>(a);

    if (account->getHostname().empty())
    	throw VoipLinkException("Account hostname is empty");

    if (account->getUsername().empty())
    	throw VoipLinkException("Account username is empty");

    // lock
    ost::MutexLock m(mutexIAX_);

    // Always use a brand new session
    if (regSession_)
        iax_destroy(regSession_);

    regSession_ = iax_session_new();

    if (!regSession_)
        _debug ("IAX: Error when generating new session for register");
    else {
        _debug ("IAX: Sending registration to %s with user %s", account->getHostname().c_str() , account->getUsername().c_str());
        int val = iax_register (regSession_, account->getHostname().data(), account->getUsername().data(), account->getPassword().data(), 120);
        _debug ("IAX: Return value: %d", val);
        // set the time-out to 15 seconds, after that, resend a registration request.
        // until we unregister.
        nextRefreshStamp_ = time (NULL) + 10;

        account->setRegistrationState (Trying);
    }
}

void
IAXVoIPLink::sendUnregister (Account *a)
{
    _debug ("IAXVoipLink: Send unregister");

    {
        ost::MutexLock m(mutexIAX_);
        if (regSession_) {
            iax_destroy (regSession_);
            regSession_ = NULL;
        }
    }

    mutexIAX_.leaveMutex();

    nextRefreshStamp_ = 0;

    IAXAccount *account = dynamic_cast<IAXAccount*>(a);
    account->setRegistrationState (Unregistered);
}

Call*
IAXVoIPLink::newOutgoingCall (const std::string& id, const std::string& toUrl)
{
    IAXCall* call = new IAXCall (id, Call::Outgoing);

    call->setPeerNumber (toUrl);
    call->initRecFileName (toUrl);

    if (iaxOutgoingInvite (call)) {
        call->setConnectionState (Call::Progressing);
        call->setState (Call::Active);
        addCall (call);
    } else {
        delete call;
        call = NULL;
    }

    return call;
}


void
IAXVoIPLink::answer (Call *c)
{
    IAXCall* call = dynamic_cast<IAXCall*>(c);

    Manager::instance().addStream (call->getCallId());

    {
        ost::MutexLock m(mutexIAX_);
        iax_answer(call->getSession());
    }

    call->setState(Call::Active);
    call->setConnectionState(Call::Connected);

    // Flush main buffer
    audiolayer_->flushMain();
}

void
IAXVoIPLink::hangup (const std::string& id)
{
    _debug ("IAXVoIPLink: Hangup");

    IAXCall* call = getIAXCall(id);
    if (call == NULL)
    	throw VoipLinkException("Could not find call");

    Manager::instance().getMainBuffer()->unBindAll(call->getCallId());

    mutexIAX_.enterMutex();
    iax_hangup (call->getSession(), (char*) "Dumped Call");
    mutexIAX_.leaveMutex();

    call->setSession(NULL);

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

    call->setSession (NULL);

    removeCall (id);
}



void
IAXVoIPLink::onhold (const std::string& id)
{
    IAXCall* call = getIAXCall (id);
    if (call == NULL)
    	throw VoipLinkException("Call does not exist");

    Manager::instance().getMainBuffer()->unBindAll (call->getCallId());

    mutexIAX_.enterMutex();
    iax_quelch_moh (call->getSession(), MUSIC_ONHOLD);
    mutexIAX_.leaveMutex();

    call->setState (Call::Hold);
}

void
IAXVoIPLink::offhold (const std::string& id)
{
    IAXCall* call = getIAXCall (id);
    if (call == NULL)
    	throw VoipLinkException("Call does not exist");

    Manager::instance().addStream (call->getCallId());

    mutexIAX_.enterMutex();
    iax_unquelch (call->getSession());
    mutexIAX_.leaveMutex();
    audiolayer_->startStream();
    call->setState (Call::Active);
}

bool
IAXVoIPLink::transfer (const std::string& id, const std::string& to)
{
    IAXCall* call = getIAXCall (id);
    CHK_VALID_CALL;

    char callto[to.length() +1];
    strcpy (callto, to.c_str());

    mutexIAX_.enterMutex();
    iax_transfer (call->getSession(), callto);
    mutexIAX_.leaveMutex();

    return true;

    // should we remove it?
    // removeCall(id);
}

bool
IAXVoIPLink::attendedTransfer(const std::string& /*transferID*/, const std::string& /*targetID*/)
{
	// TODO implement attended transfer for IAX
	return false;
}

bool
IAXVoIPLink::refuse (const std::string& id)
{
    IAXCall* call = getIAXCall (id);
    CHK_VALID_CALL;

    mutexIAX_.enterMutex();
    iax_reject (call->getSession(), (char*) "Call rejected manually.");
    mutexIAX_.leaveMutex();

    removeCall (id);

    return true;
}


bool
IAXVoIPLink::carryingDTMFdigits (const std::string& id, char code)
{
    IAXCall* call = getIAXCall (id);
    CHK_VALID_CALL;

    mutexIAX_.enterMutex();
    iax_send_dtmf (call->getSession(), code);
    mutexIAX_.leaveMutex();

    return true;
}

bool
IAXVoIPLink::sendTextMessage (sfl::InstantMessaging *module,
        const std::string& callID, const std::string& message,
        const std::string& /*from*/)
{
    IAXCall* call = getIAXCall (callID);
    CHK_VALID_CALL;

    // Must active the mutex for this session
    mutexIAX_.enterMutex();

    module->send_iax_message (call->getSession(), callID, message.c_str());

    // iax_send_text (call->getSession(), message.c_str());
    mutexIAX_.leaveMutex();

    return true;
}


std::string
IAXVoIPLink::getCurrentCodecName(Call *c) const
{
    IAXCall *call = dynamic_cast<IAXCall*>(c);
    sfl::Codec *audioCodec = Manager::instance().audioCodecFactory.getCodec(call->getAudioCodec());
    return audioCodec ? audioCodec->getMimeSubtype() : "";
}


bool
IAXVoIPLink::iaxOutgoingInvite (IAXCall* call)
{
    ost::MutexLock m(mutexIAX_);

    iax_session *newsession = iax_session_new();

    if (!newsession) {
        _debug ("IAX Error: Can't make new session for a new call");
        return false;
    }

    call->setSession(newsession);

    IAXAccount *account = dynamic_cast<IAXAccount *>(Manager::instance().getAccount(accountID_));
    std::string username(account->getUsername());
    std::string strNum(username + ":" + account->getPassword() + "@" + account->getHostname() + "/" + call->getPeerNumber());

    /** @todo Make preference dynamic, and configurable */
    int audio_format_preferred = call->getFirstMatchingFormat(call->getSupportedFormat(accountID_), accountID_);
    int audio_format_capability = call->getSupportedFormat(accountID_);

    _debug ("IAX New call: %s", strNum.c_str());
    iax_call(newsession, username.c_str(), username.c_str(), strNum.c_str(),
              NULL, 0, audio_format_preferred, audio_format_capability);

    return true;
}


IAXCall*
IAXVoIPLink::iaxFindCallBySession (struct iax_session* session)
{
    // access to callMap shoud use that
    // the code below is like findSIPCallWithCid()
    ost::MutexLock m(_callMapMutex);
    for (CallMap::const_iterator iter = _callMap.begin(); iter != _callMap.end(); ++iter) {
        IAXCall* call = dynamic_cast<IAXCall*> (iter->second);
        if (call and call->getSession() == session)
            return call;
    }

    return NULL; // not found
}

void
IAXVoIPLink::iaxHandleCallEvent (iax_event* event, IAXCall* call)
{
    // call should not be 0
    // note activity?
    //
    std::string id = call->getCallId();

    switch (event->etype) {

        case IAX_EVENT_HANGUP:

            if (Manager::instance().isCurrentCall (id)) {
                // stop audio
                // audiolayer->stopStream();
            }

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
            // Call accepted over there by the computer, not the user yet.
            _debug ("IAX_EVENT_ACCEPT: codec format: %d", event->ies.format);

            if (event->ies.format)
                call->setFormat (event->ies.format);

            break;

        case IAX_EVENT_ANSWER:
        case IAX_EVENT_TRANSFER:
            if (call->getConnectionState() == Call::Connected)
                break;

            Manager::instance().addStream (call->getCallId());

            call->setConnectionState (Call::Connected);
            call->setState (Call::Active);
            // audiolayer->startStream();

            _debug ("IAX_EVENT_ANSWER: codec format: %d", event->ies.format);

            // Should not get here, should have been set in EVENT_ACCEPT
            if (event->ies.format)
                call->setFormat (event->ies.format);

            Manager::instance().peerAnsweredCall (id);

            // start audio here?
            audiolayer_->startStream();
            audiolayer_->flushMain();

            break;

        case IAX_EVENT_BUSY:
            call->setConnectionState (Call::Connected);
            call->setState (Call::Busy);
            Manager::instance().callBusy (id);
            removeCall (id);
            break;

        case IAX_EVENT_VOICE:
            //if (!audiolayer->isCaptureActive ())
            //  audiolayer->startStream ();
            // _debug("IAX_EVENT_VOICE: ");
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
            break;

        case IAX_EVENT_PONG:
            break;

        case IAX_EVENT_URL:

            if (Manager::instance().getConfigString (HOOKS, URLHOOK_IAX2_ENABLED) == "1") {
                if (*event->data) {
                    _debug ("> IAX_EVENT_URL received: %s", event->data);
                    UrlHook::runAction (Manager::instance().getConfigString (HOOKS, URLHOOK_COMMAND), (char*) event->data);
                }
            }

            break;

        case IAX_EVENT_TIMEOUT:
            break;

        default:
            _debug ("iaxHandleCallEvent: Unknown event type (in call event): %d", event->etype);

    }
}


/* Handle audio event, VOICE packet received */
void
IAXVoIPLink::iaxHandleVoiceEvent (iax_event* event, IAXCall* call)
{
    // Skip this empty packet.
    if (!event->datalen)
        return;

    if (audiolayer_) {
        _debug ("IAX: incoming audio, but no sound card open");
        return;
    }

    sfl::AudioCodec *audioCodec = static_cast<sfl::AudioCodec *>(Manager::instance().audioCodecFactory.getCodec(call->getAudioCodec()));
    if (!audioCodec)
        return;

	Manager::instance().getMainBuffer()->setInternalSamplingRate(audioCodec->getClockRate());
	unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();

	// On-the-fly codec changing (normally, when we receive a full packet)
	// as per http://tools.ietf.org/id/draft-guy-iax-03.txt
	// - subclass holds the voiceformat property.

	if (event->subclass && event->subclass != call->getFormat()) {
		_debug ("iaxHandleVoiceEvent: no format found in call setting it to %i", event->subclass);
		call->setFormat (event->subclass);
	}

	unsigned char *data = (unsigned char*) event->data;
	unsigned int size = event->datalen;

	// Decode data with relevant codec
	unsigned int max = audioCodec->getClockRate() * 20 / 1000;

	if (size > max) {
		_debug ("The size %d is bigger than expected %d. Packet cropped. Ouch!", size, max);
		size = max;
	}

	int samples = audioCodec->decode (decData, data , size);
	int outSize = samples * sizeof(SFLDataFormat);
	unsigned int audioRate = audioCodec->getClockRate();
	if (audioRate != mainBufferSampleRate) {
		outSize = (double)outSize * (mainBufferSampleRate / audioRate);
		converter_->resample (decData, resampledData, mainBufferSampleRate, audioRate, samples);
		audiolayer_->getMainBuffer()->putData (resampledData, outSize, call->getCallId());
	} else
		audiolayer_->getMainBuffer()->putData (decData, outSize, call->getCallId());
}

/**
 * Handle the registration process
 */
void
IAXVoIPLink::iaxHandleRegReply (iax_event* event)
{
    IAXAccount *account = dynamic_cast<IAXAccount *>(Manager::instance().getAccount(accountID_));

    if (event->etype == IAX_EVENT_REGREJ) {
        /* Authentication failed! */
        {
            ost::MutexLock m(mutexIAX_);
            iax_destroy(regSession_);
        }
        regSession_ = NULL;
        // Update the account registration state
        account->setRegistrationState (ErrorAuth);
    }

    else if (event->etype == IAX_EVENT_REGACK) {
        /* Authentication succeeded */
        {
            ost::MutexLock m(mutexIAX_);
            iax_destroy (regSession_);
        }
        regSession_ = NULL;

        // I mean, save the timestamp, so that we re-register again in the REFRESH time.
        // Defaults to 60, as per draft-guy-iax-03.
        nextRefreshStamp_ = time (NULL) + (event->ies.refresh ? event->ies.refresh : 60);
        account->setRegistrationState (Registered);
    }
}

int IAXVoIPLink::processIAXMsgCount (int msgcount)
{
    // IAX sends the message count under a specific format:
    //                       1
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |      0x18     |      0x02     |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |  Old messages |  New messages |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    // For now we just need the new messages informations.
    // Thus:
    // 0 <= msgcount <= 255   => msgcount new messages, 0 old messages
    // msgcount >= 256        => msgcount/256 old messages , msgcount%256 new messages (RULES)

    return msgcount % 256;
}


void
IAXVoIPLink::iaxHandlePrecallEvent (iax_event* event)
{
    IAXCall* call = NULL;
    std::string id;

    switch (event->etype) {
        case IAX_EVENT_REGACK:

        case IAX_EVENT_REGREJ:
            _debug ("IAX Registration Event in a pre-call setup");
            break;

        case IAX_EVENT_REGREQ:
            // Received when someone wants to register to us!?!
            // Asterisk receives and answers to that, not us, we're a phone.
            _debug ("Registration by a peer, don't allow it");
            break;

        case IAX_EVENT_CONNECT:
            // We've got an incoming call! Yikes!
            _debug ("> IAX_EVENT_CONNECT (receive)");

            id = Manager::instance().getNewCallID();

            call = new IAXCall (id, Call::Incoming);

            // Setup the new IAXCall
            // Associate the call to the session.
            call->setSession (event->session);

            call->setConnectionState (Call::Progressing);


            if (event->ies.calling_number)
                call->setPeerNumber (std::string (event->ies.calling_number));

            if (event->ies.calling_name)
                call->setPeerName (std::string (event->ies.calling_name));

            // if peerNumber exist append it to the name string
            call->initRecFileName (std::string (event->ies.calling_number));

            if (Manager::instance().incomingCall (call, accountID_)) {
                /** @todo Faudra considérer éventuellement le champ CODEC PREFS pour
                 * l'établissement du codec de transmission */

                // Remote lists its capabilities
                int format = call->getFirstMatchingFormat (event->ies.capability, accountID_);
                // Remote asks for preferred codec voiceformat
                int pref_format = call->getFirstMatchingFormat (event->ies.format, accountID_);

                // Priority to remote's suggestion. In case it's a forwarding, no transcoding
                // will be needed from the server, thus less latency.

                if (pref_format)
                    format = pref_format;

                iax_accept (event->session, format);

                iax_ring_announce (event->session);

                addCall (call);

                // Set the audio codec now, to avoid non-set codec problem later
                call->setFormat (format);

            } else {
                // reject call, unable to add it
                iax_reject (event->session, (char*) "Error ringing user.");
                delete call;
            }

            break;

        case IAX_EVENT_HANGUP:
            // Remote peer hung up
            call = iaxFindCallBySession (event->session);
            id = call->getCallId();
            _debug ("IAXVoIPLink::hungup::iaxHandlePrecallEvent");
            Manager::instance().peerHungupCall (id);
            removeCall (id);
            break;

        case IAX_EVENT_TIMEOUT: // timeout for an unknown session
            break;

        case IAX_IE_MSGCOUNT:
            break;

        default:
            _debug ("IAXVoIPLink::iaxHandlePrecallEvent: Unknown event type (in precall): %d", event->etype);
    }
}

void IAXVoIPLink::updateAudiolayer (void)
{
   ost::MutexLock m(mutexIAX_);
   audiolayer_ = Manager::instance().getAudioDriver();
}
