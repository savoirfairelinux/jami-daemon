/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include "manager.h"

namespace sfl {
AudioRtpSession::AudioRtpSession(SIPCall * sipcall, RtpMethod type, ost::RTPDataQueue *queue, ost::Thread *thread) :
    AudioRtpRecordHandler(sipcall)
    , _ca(sipcall)
    , _type(type)
    , _timestamp(0)
    , _timestampIncrement(0)
    , _timestampCount(0)
    , _isStarted(false)
    , _queue(queue)
    , _thread(thread)
{
    assert(_ca);
    _queue->setTypeOfService(ost::RTPDataQueue::tosEnhanced);
}

AudioRtpSession::~AudioRtpSession()
{
    _queue->disableStack();
}

void AudioRtpSession::updateSessionMedia(AudioCodec *audioCodec)
{
    int lastSamplingRate = _audioRtpRecord._codecSampleRate;

    setSessionMedia(audioCodec);

    Manager::instance().audioSamplingRateChanged(_audioRtpRecord._codecSampleRate);

    if (lastSamplingRate != _audioRtpRecord._codecSampleRate) {
        _debug("AudioRtpSession: Update noise suppressor with sampling rate %d and frame size %d", getCodecSampleRate(), getCodecFrameSize());
        initNoiseSuppress();
    }

}

void AudioRtpSession::setSessionMedia(AudioCodec *audioCodec)
{
    setRtpMedia(audioCodec);

    // store codec info locally
    int payloadType = getCodecPayloadType();
    int frameSize = getCodecFrameSize();
    int smplRate = getCodecSampleRate();
    bool dynamic = getHasDynamicPayload();

    // G722 requires timestamp to be incremented at 8kHz
    if (payloadType == g722PayloadType)
        _timestampIncrement = g722RtpTimeincrement;
    else
        _timestampIncrement = frameSize;

    _debug("AudioRptSession: Codec payload: %d", payloadType);
    _debug("AudioSymmetricRtpSession: Codec sampling rate: %d", smplRate);
    _debug("AudioSymmetricRtpSession: Codec frame size: %d", frameSize);
    _debug("AudioSymmetricRtpSession: RTP timestamp increment: %d", _timestampIncrement);

    if (payloadType == g722PayloadType) {
        _debug("AudioSymmetricRtpSession: Setting G722 payload format");
        _queue->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) payloadType, g722RtpClockRate));
    } else {
        if (dynamic) {
            _debug("AudioSymmetricRtpSession: Setting dynamic payload format");
            _queue->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) payloadType, smplRate));
        } else {
            _debug("AudioSymmetricRtpSession: Setting static payload format");
            _queue->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) payloadType));
        }
    }

    if (_type != Zrtp)
        _ca->setRecordingSmplRate(getCodecSampleRate());
}

void AudioRtpSession::sendDtmfEvent()
{
    ost::RTPPacket::RFC2833Payload payload;

    payload.event = _audioRtpRecord._dtmfQueue.front();
    payload.ebit = false; // end of event bit
    payload.rbit = false; // reserved bit
    payload.duration = 1; // duration for this event

    _audioRtpRecord._dtmfQueue.pop_front();

    _debug("AudioRtpSession: Send RTP Dtmf (%d)", payload.event);

    _timestamp += (_type == Zrtp) ? 160 : _timestampIncrement;

    // discard equivalent size of audio
    processDataEncode();

    // change Payload type for DTMF payload
    _queue->setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) getDtmfPayloadType(), 8000));

    _queue->setMark(true);
    _queue->sendImmediate(_timestamp, (const unsigned char *)(&payload), sizeof(payload));
    _queue->setMark(false);

    // get back the payload to audio
    _queue->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) getCodecPayloadType()));
}


void AudioRtpSession::receiveSpeakerData()
{
    const ost::AppDataUnit* adu = _queue->getData(_queue->getFirstTimestamp());

    if (!adu)
        return;

    unsigned char* spkrDataIn = (unsigned char*) adu->getData(); // data in char
    unsigned int size = adu->getSize(); // size in char

    // DTMF over RTP, size must be over 4 in order to process it as voice data
    if (size > 4)
        processDataDecode(spkrDataIn, size, adu->getType());

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
        _queue->putData(_timestamp, getMicDataEncoded(), compSize);

    // putData put the data on RTP queue, sendImmediate bypass this queue
    _queue->sendImmediate(_timestamp, getMicDataEncoded(), compSize);
}


void AudioRtpSession::setSessionTimeouts(void)
{
    _debug("AudioRtpSession: Set session scheduling timeout (%d) and expireTimeout (%d)", sfl::schedulingTimeout, sfl::expireTimeout);

    _queue->setSchedulingTimeout(sfl::schedulingTimeout);
    _queue->setExpireTimeout(sfl::expireTimeout);
}

void AudioRtpSession::setDestinationIpAddress(void)
{
    _info("AudioRtpSession: Setting IP address for the RTP session");

    // Store remote ip in case we would need to forget current destination
    _remote_ip = ost::InetHostAddress(_ca->getLocalSDP()->getRemoteIP().c_str());

    if (!_remote_ip) {
        _warn("AudioRtpSession: Target IP address (%s) is not correct!",
              _ca->getLocalSDP()->getRemoteIP().data());
        return;
    }

    // Store remote port in case we would need to forget current destination
    _remote_port = (unsigned short) _ca->getLocalSDP()->getRemoteAudioPort();

    _info("AudioRtpSession: New remote address for session: %s:%d",
          _ca->getLocalSDP()->getRemoteIP().data(), _remote_port);

    if (!_queue->addDestination(_remote_ip, _remote_port)) {
        _warn("AudioRtpSession: Can't add new destination to session!");
        return;
    }
}

void AudioRtpSession::updateDestinationIpAddress(void)
{
    _debug("AudioRtpSession: Update destination ip address");

    // Destination address are stored in a list in ccrtp
    // This method remove the current destination entry

    if (!_queue->forgetDestination(_remote_ip, _remote_port, _remote_port+1))
        _debug("AudioRtpSession: Did not remove previous destination");

    // new destination is stored in call
    // we just need to recall this method
    setDestinationIpAddress();
}


int AudioRtpSession::startRtpThread(AudioCodec* audiocodec)
{
    if (_isStarted)
        return 0;

    _debug("AudioSymmetricRtpSession: Starting main thread");

    _isStarted = true;
    setSessionTimeouts();
    setSessionMedia(audiocodec);
    initBuffers();
    initNoiseSuppress();

    _queue->enableStack();
    int ret = _thread->start();

    if (_type == Zrtp)
        return ret;

    AudioSymmetricRtpSession *self = dynamic_cast<AudioSymmetricRtpSession*>(this);
    assert(self);
    return self->startSymmetricRtpThread();
}


bool AudioRtpSession::onRTPPacketRecv(ost::IncomingRTPPkt&)
{
    receiveSpeakerData();
    return true;
}

}
