/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#include "AudioRtpSession.h"

namespace sfl
{

AudioRtpSession::AudioRtpSession (ManagerImpl * manager, SIPCall * sipcall) :
		ost::SymmetricRTPSession (ost::InetHostAddress (sipcall->getLocalIp().c_str()), sipcall->getLocalAudioPort())
																			, _time (new ost::Time())
        																	, _mainloopSemaphore (0)
        																	, _manager (manager)
        																	, _timestamp (0)
        																	, _timestampIncrement (0)
        																	, _timestampCount (0)
        																	, _countNotificationTime (0)
        																	, _ca (sipcall)
{
	setCancel (cancelDefault);

    assert (_ca);

    _info ("AudioRtpSession: Local audio port %i will be used", _ca->getLocalAudioPort());

}

AudioRtpSession::~AudioRtpSession()
{
    _debug ("AudioRtpSession: Delete AudioRtpSession instance");

    try {
    	terminate();
    } catch (...) {
        _debugException ("AudioRtpSession: Thread destructor didn't terminate correctly");
        throw;
    }

    _manager->getAudioDriver()->getMainBuffer()->unBindAll (_ca->getCallId());

    if(_time)
    	delete _time;
    _time = NULL;

}

void AudioRtpSession::setSessionTimeouts (void)
{
	setSchedulingTimeout (sfl::schedulingTimeout);
	setExpireTimeout (sfl::expireTimeout);
}

void AudioRtpSession::setSessionMedia (AudioCodec* audioCodec)
{
	// set internal codec info for this session
	setRtpMedia(audioCodec);

	// store codec info locally
	int payloadType = getCodecPayloadType();
	int frameSize = getCodecFrameSize();
	int smplRate = getCodecSampleRate();
	bool dynamic = getHasDynamicPayload();

    // G722 requires timestamp to be incremented at 8 kHz
    if (payloadType == 9)
        _timestampIncrement = 160;
    else
        _timestampIncrement = frameSize;

    _debug ("AudioRtpSession: Codec sampling rate: %d", smplRate);
    _debug ("AudioRtpSession: Codec frame size: %d", frameSize);
    _debug ("AudioRtpSession: RTP timestamp increment: %d", _timestampIncrement);

    // Even if specified as a 16 kHz codec, G722 requires rtp sending rate to be 8 kHz
    if (payloadType == 9) {
        _debug ("AudioRtpSession: Setting G722 payload format");
        setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) payloadType, smplRate));
    } else if (dynamic) {
        _debug ("AudioRtpSession: Setting dynamic payload format");
        setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) payloadType, smplRate));
    } else if (dynamic && payloadType != 9) {
        _debug ("AudioRtpSession: Setting static payload format");
        setPayloadFormat (ost::StaticPayloadFormat ( (ost::StaticPayloadType) payloadType));
    }

}

void AudioRtpSession::setDestinationIpAddress (void)
{
    if (_ca == NULL) {
        _error ("AudioRtpSession: Sipcall is gone.");
        throw AudioRtpSessionException();
    }

    _info ("AudioRtpSession: Setting IP address for the RTP session");

    // Store remote ip in case we would need to forget current destination
    _remote_ip = ost::InetHostAddress (_ca->getLocalSDP()->get_remote_ip().c_str());

    if (!_remote_ip) {
        _warn ("AudioRtpSession: Target IP address (%s) is not correct!",
               _ca->getLocalSDP()->get_remote_ip().data());
        return;
    }

    // Store remote port in case we would need to forget current destination
    _remote_port = (unsigned short) _ca->getLocalSDP()->get_remote_audio_port();

    _info ("AudioRtpSession: New remote address for session: %s:%d",
           _ca->getLocalSDP()->get_remote_ip().data(), _remote_port);

    if (!addDestination (_remote_ip, _remote_port)) {
        _warn ("AudioRtpSession: Can't add new destination to session!");
        return;
    }
}

void AudioRtpSession::updateDestinationIpAddress (void)
{
    // Destination address are stored in a list in ccrtp
    // This method remove the current destination entry

    if (!forgetDestination (_remote_ip, _remote_port, _remote_port+1))
        _warn ("AudioRtpSession: Could not remove previous destination");

    // new destination is stored in call
    // we just need to recall this method
    setDestinationIpAddress();
}

void AudioRtpSession::sendDtmfEvent (sfl::DtmfEvent *dtmf)
{
    _debug ("AudioRtpSession: Send Dtmf %d", getEventQueueSize());

    _timestamp += 160;

    // discard equivalent size of audio
    processDataEncode();

    // change Payload type for DTMF payload
    setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) 101, 8000));

    // Set marker in case this is a new Event
    if (dtmf->newevent)
        setMark (true);

    putData (_timestamp, (const unsigned char*) (& (dtmf->payload)), sizeof (ost::RTPPacket::RFC2833Payload));

    // This is no more a new event
    if (dtmf->newevent) {
        dtmf->newevent = false;
        setMark (false);
    }

    // get back the payload to audio
    setPayloadFormat (ost::StaticPayloadFormat ( (ost::StaticPayloadType) getCodecPayloadType()));

    // decrease length remaining to process for this event
    dtmf->length -= 160;

    dtmf->payload.duration += 1;

    // next packet is going to be the last one
    if ( (dtmf->length - 160) < 160)
        dtmf->payload.ebit = true;

    if (dtmf->length < 160) {
        delete dtmf;
        getEventQueue()->pop_front();
    }
}

bool onRTPPacketRecv (ost::IncomingRTPPkt&)
{
    _debug ("AudioRtpSession: onRTPPacketRecv");

    return true;
}


void AudioRtpSession::sendMicData()
{
    // Increment timestamp for outgoing packet
    _timestamp += _timestampIncrement;

    int compSize = processDataEncode();

    // putData put the data on RTP queue, sendImmediate bypass this queue
    putData (_timestamp, getEncodedData(), compSize);
}


void AudioRtpSession::receiveSpeakerData ()
{
    const ost::AppDataUnit* adu = NULL;

    int packetTimestamp = getFirstTimestamp();

    adu = getData (packetTimestamp);

    if (!adu) {
        return;
    }

    unsigned char* spkrDataIn = NULL;
    unsigned int size = 0;

    if (adu) {

        spkrDataIn  = (unsigned char*) adu->getData(); // data in char
        size = adu->getSize(); // size in char

    } else {
        _debug ("AudioRtpSession: No RTP packet available");
    }

    // DTMF over RTP, size must be over 4 in order to process it as voice data
    if (size > 4) {
        processDataDecode (spkrDataIn, size);
    }

    delete adu;
}


int AudioRtpSession::startRtpThread (AudioCodec* audiocodec)
{
    _debug ("RTP: Starting main thread");
    setSessionTimeouts();
    setSessionMedia (audiocodec);
    initBuffers();
    return start (_mainloopSemaphore);
}

void AudioRtpSession::run ()
{

    // Timestamp must be initialized randomly
    _timestamp = getCurrentTimestamp();

    int threadSleep = 0;

    if (getCodecSampleRate() != 0) {
        threadSleep = (getCodecFrameSize() * 1000) / getCodecSampleRate();
    } else {
    	// TODO should not be dependent of audio layer frame size
        threadSleep = getAudioLayerFrameSize();
    }

    TimerPort::setTimer (threadSleep);

    // Set recording sampling rate
    _ca->setRecordingSmplRate (getCodecSampleRate());

    // Start audio stream (if not started) AND flush all buffers (main and urgent)
    _manager->getAudioDriver()->startStream();
    startRunning();

    _debug ("AudioRtpSession: Entering mainloop for call %s",_ca->getCallId().c_str());

    while (!testCancel()) {

        // Reset timestamp to make sure the timing information are up to date
        if (_timestampCount > RTP_TIMESTAMP_RESET_FREQ) {
            _timestamp = getCurrentTimestamp();
            _timestampCount = 0;
        }

        _timestampCount++;

        _manager->getAudioLayerMutex()->enter();

        // TODO should not be linked to audio layer here
        // converterSamplingRate = _audiolayer->getMainBuffer()->getInternalSamplingRate();
        // _converterSamplingRate = _manager->getAudioDriver()->getMainBuffer()->getInternalSamplingRate();

        // Send session
        if (getEventQueueSize() > 0) {
            sendDtmfEvent (getEventQueue()->front());
        } else {
            sendMicData ();
        }

        // Recv session
        // TODO should not be called here anymore
        // receiveSpeakerData ();

        _manager->getAudioLayerMutex()->leave();

        // Let's wait for the next transmit cycle
        Thread::sleep (TimerPort::getTimer());
        TimerPort::incTimer (threadSleep);
    }

    _debug ("AudioRtpSession: Left main loop for call %s", _ca->getCallId().c_str());
}

}
