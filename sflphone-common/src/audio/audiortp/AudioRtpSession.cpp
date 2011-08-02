/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include "AudioSymmetricRtpSession.h"

#include "sip/sdp.h"
#include "audio/audiolayer.h"
#include <ccrtp/rtp.h>
#include <ccrtp/oqueue.h>

namespace sfl
{
AudioRtpSession::AudioRtpSession (SIPCall * sipcall, RtpMethod type, ost::RTPDataQueue *queue, ost::Thread *thread) :
					AudioRtpRecordHandler (sipcall)
					, _ca (sipcall)
					, _timestamp (0)
					, _timestampIncrement (0)
					, _timestampCount (0)
                    , _isStarted(false)
					, _type(type)
					, _queue(queue)
					, _thread(thread)
{
    assert (_ca);
    _queue->setTypeOfService (ost::RTPDataQueue::tosEnhanced);
}

AudioRtpSession::~AudioRtpSession()
{
    _info ("AudioRtpSession: Delete AudioRtpSession instance");
}

void AudioRtpSession::updateSessionMedia (AudioCodec *audioCodec)
{
    _debug ("AudioSymmetricRtpSession: Update session media");

    // Update internal codec for this session
    updateRtpMedia (audioCodec);

    int payloadType = getCodecPayloadType();
    int frameSize = getCodecFrameSize();
    int smplRate = getCodecSampleRate();
    int dynamic = getHasDynamicPayload();

    // G722 requires timetamp to be incremented at 8khz
    if (payloadType == g722PayloadType)
        _timestampIncrement = g722RtpTimeincrement;
    else
        _timestampIncrement = frameSize;

    _debug ("AudioRptSession: Codec payload: %d", payloadType);
    _debug ("AudioSymmetricRtpSession: Codec sampling rate: %d", smplRate);
    _debug ("AudioSymmetricRtpSession: Codec frame size: %d", frameSize);
    _debug ("AudioSymmetricRtpSession: RTP timestamp increment: %d", _timestampIncrement);


    if (payloadType == g722PayloadType) {
        _debug ("AudioSymmetricRtpSession: Setting G722 payload format");
        _queue->setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) payloadType, g722RtpClockRate));
    } else {
        if (dynamic) {
            _debug ("AudioSymmetricRtpSession: Setting dynamic payload format");
            _queue->setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) payloadType, smplRate));
        } else {
            _debug ("AudioSymmetricRtpSession: Setting static payload format");
            _queue->setPayloadFormat (ost::StaticPayloadFormat ( (ost::StaticPayloadType) payloadType));
        }
    }

    if (_type != Zrtp) {
		_ca->setRecordingSmplRate (getCodecSampleRate());
		_timestamp = _queue->getCurrentTimestamp();
    }
}

void AudioRtpSession::setSessionMedia (AudioCodec *audioCodec)
{
    _debug ("AudioSymmetricRtpSession: Set session media");

    // set internal codec info for this session
    setRtpMedia (audioCodec);

    // store codec info locally
    int payloadType = getCodecPayloadType();
    int frameSize = getCodecFrameSize();
    int smplRate = getCodecSampleRate();
    bool dynamic = getHasDynamicPayload();

    // G722 requires timestamp to be incremented at 8 kHz
    if (payloadType == g722PayloadType)
        _timestampIncrement = g722RtpTimeincrement;
    else
        _timestampIncrement = frameSize;

    _debug ("AudioRptSession: Codec payload: %d", payloadType);
    _debug ("AudioSymmetricRtpSession: Codec sampling rate: %d", smplRate);
    _debug ("AudioSymmetricRtpSession: Codec frame size: %d", frameSize);
    _debug ("AudioSymmetricRtpSession: RTP timestamp increment: %d", _timestampIncrement);

    if (payloadType == g722PayloadType) {
        _debug ("AudioSymmetricRtpSession: Setting G722 payload format");
        _queue->setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) payloadType, g722RtpClockRate));
    } else {
        if (dynamic) {
            _debug ("AudioSymmetricRtpSession: Setting dynamic payload format");
            _queue->setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) payloadType, smplRate));
        } else {
            _debug ("AudioSymmetricRtpSession: Setting static payload format");
            _queue->setPayloadFormat (ost::StaticPayloadFormat ( (ost::StaticPayloadType) payloadType));
        }
    }

    if (_type != Zrtp) {
		_ca->setRecordingSmplRate (getCodecSampleRate());
	}
}

void AudioRtpSession::sendDtmfEvent (sfl::DtmfEvent *dtmf)
{
	const int increment = (_type == Zrtp) ? 160 : _timestampIncrement;
    _debug ("AudioRtpSession: Send Dtmf");

    _timestamp += increment;

    // discard equivalent size of audio
    processDataEncode();

    // change Payload type for DTMF payload
    _queue->setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) getDtmfPayloadType(), 8000));

    // Set marker in case this is a new Event
    if (dtmf->newevent)
        _queue->setMark (true);

    // putData (_timestamp, (const unsigned char*) (& (dtmf->payload)), sizeof (ost::RTPPacket::RFC2833Payload));
    _queue->sendImmediate (_timestamp, (const unsigned char *) (& (dtmf->payload)), sizeof (ost::RTPPacket::RFC2833Payload));

    // This is no more a new event
    if (dtmf->newevent) {
        dtmf->newevent = false;
        _queue->setMark (false);
    }

    // get back the payload to audio
    _queue->setPayloadFormat (ost::StaticPayloadFormat ( (ost::StaticPayloadType) getCodecPayloadType()));

    // decrease length remaining to process for this event
    dtmf->length -= increment;

    dtmf->payload.duration++;


    // next packet is going to be the last one
    if ( (dtmf->length - increment) < increment)
        dtmf->payload.ebit = true;

    if (dtmf->length < increment) {
        delete dtmf;
        getEventQueue()->pop_front();
    }
}


void AudioRtpSession::receiveSpeakerData ()
{
    const ost::AppDataUnit* adu = NULL;

    int packetTimestamp = _queue->getFirstTimestamp();

    adu = _queue->getData (packetTimestamp);

    if (!adu) {
        return;
    }

    unsigned char* spkrDataIn = NULL;
    unsigned int size = 0;

    spkrDataIn  = (unsigned char*) adu->getData(); // data in char
    size = adu->getSize(); // size in char

    // DTMF over RTP, size must be over 4 in order to process it as voice data
    if (size > 4) {
        processDataDecode (spkrDataIn, size);
    }

    delete adu;
}



void AudioRtpSession::sendMicData()
{
    int compSize = processDataEncode();

    // if no data return
    if (!compSize)
        return;

    // Increment timestamp for outgoing packet
    _timestamp += _timestampIncrement;

    if (_type == Zrtp)
    	_queue->putData (_timestamp, getMicDataEncoded(), compSize);
    // putData put the data on RTP queue, sendImmediate bypass this queue
    _queue->sendImmediate (_timestamp, getMicDataEncoded(), compSize);
}


void AudioRtpSession::setSessionTimeouts (void)
{
    _debug ("AudioRtpSession: Set session scheduling timeout (%d) and expireTimeout (%d)", sfl::schedulingTimeout, sfl::expireTimeout);

    _queue->setSchedulingTimeout (sfl::schedulingTimeout);
    _queue->setExpireTimeout (sfl::expireTimeout);
}

void AudioRtpSession::setDestinationIpAddress (void)
{
    _info ("AudioRtpSession: Setting IP address for the RTP session");

    // Store remote ip in case we would need to forget current destination
    _remote_ip = ost::InetHostAddress (_ca->getLocalSDP()->getRemoteIP().c_str());

    if (!_remote_ip) {
        _warn ("AudioRtpSession: Target IP address (%s) is not correct!",
               _ca->getLocalSDP()->getRemoteIP().data());
        return;
    }

    // Store remote port in case we would need to forget current destination
    _remote_port = (unsigned short) _ca->getLocalSDP()->getRemoteAudioPort();

    _info ("AudioRtpSession: New remote address for session: %s:%d",
           _ca->getLocalSDP()->getRemoteIP().data(), _remote_port);

    if (!_queue->addDestination (_remote_ip, _remote_port)) {
        _warn ("AudioRtpSession: Can't add new destination to session!");
        return;
    }
}

void AudioRtpSession::updateDestinationIpAddress (void)
{
    _debug ("AudioRtpSession: Update destination ip address");

    // Destination address are stored in a list in ccrtp
    // This method remove the current destination entry

    if (!_queue->forgetDestination (_remote_ip, _remote_port, _remote_port+1))
        _warn ("AudioRtpSession: Could not remove previous destination");

    // new destination is stored in call
    // we just need to recall this method
    setDestinationIpAddress();
}


int AudioRtpSession::startRtpThread (AudioCodec* audiocodec)
{
    if (_isStarted)
        return 0;

    _debug ("AudioSymmetricRtpSession: Starting main thread");

    _isStarted = true;
    setSessionTimeouts();
    setSessionMedia (audiocodec);
    initBuffers();
    initNoiseSuppress();

    _queue->enableStack();
    int ret = _thread->start();
	if (_type == Zrtp)
		return ret;

    return static_cast<AudioSymmetricRtpSession*>(this)->startSymmetricRtpThread();
}

void AudioRtpSession::stopRtpThread ()
{
    _debug ("AudioSymmetricRtpSession: Stoping main thread");

    if (_type != Zrtp) {
        static_cast<AudioSymmetricRtpSession*>(this)->stopSymmetricRtpThread();
    }

    _queue->disableStack();
}


bool AudioRtpSession::onRTPPacketRecv (ost::IncomingRTPPkt&)
{
    receiveSpeakerData();

    return true;
}

}
