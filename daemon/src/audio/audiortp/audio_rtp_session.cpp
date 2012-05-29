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

#include "audio_rtp_session.h"
#include "logger.h"
#include "sip/sdp.h"
#include "sip/sipcall.h"
#include <ccrtp/oqueue.h>
#include "manager.h"

namespace sfl {
AudioRtpSession::AudioRtpSession(SIPCall &call, ost::RTPDataQueue &queue, ost::Thread &thread) :
    AudioRtpRecordHandler(call)
    , call_(call)
    , timestamp_(0)
    , timestampIncrement_(0)
    , queue_(queue)
    , isStarted_(false)
    , remote_ip_()
    , remote_port_(0)
    , timestampCount_(0)
    , thread_(thread)
{
    queue_.setTypeOfService(ost::RTPDataQueue::tosEnhanced);
}

AudioRtpSession::~AudioRtpSession()
{
    queue_.disableStack();
}

void AudioRtpSession::updateSessionMedia(AudioCodec &audioCodec)
{
    int lastSamplingRate = audioRtpRecord_.codecSampleRate_;

    setSessionMedia(audioCodec);

    Manager::instance().audioSamplingRateChanged(audioRtpRecord_.codecSampleRate_);

    if (lastSamplingRate != audioRtpRecord_.codecSampleRate_) {
        DEBUG("Update noise suppressor with sampling rate %d and frame size %d",
              getCodecSampleRate(), getCodecFrameSize());
        initNoiseSuppress();
    }
}

void AudioRtpSession::setSessionMedia(AudioCodec &audioCodec)
{
    setRtpMedia(&audioCodec);

    // store codec info locally
    int payloadType = getCodecPayloadType();
    int frameSize = getCodecFrameSize();
    int smplRate = getCodecSampleRate();
    bool dynamic = getHasDynamicPayload();

    // G722 requires timestamp to be incremented at 8kHz
    if (payloadType == g722PayloadType)
        timestampIncrement_ = g722RtpTimeincrement;
    else
        timestampIncrement_ = frameSize;

    DEBUG("Codec payload: %d", payloadType);
    DEBUG("Codec sampling rate: %d", smplRate);
    DEBUG("Codec frame size: %d", frameSize);
    DEBUG("RTP timestamp increment: %d", timestampIncrement_);

    if (payloadType == g722PayloadType) {
        DEBUG("Setting G722 payload format");
        queue_.setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) payloadType, g722RtpClockRate));
    } else {
        if (dynamic) {
            DEBUG("Setting dynamic payload format");
            queue_.setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) payloadType, smplRate));
        } else {
            DEBUG("Setting static payload format");
            queue_.setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) payloadType));
        }
    }
}

void AudioRtpSession::sendDtmfEvent()
{
    DTMFEvent &dtmf(audioRtpRecord_.dtmfQueue_.front());
    DEBUG("Send RTP Dtmf (%d)", dtmf.payload.event);

    const int increment = getIncrementForDTMF();
    timestamp_ += increment;

    // discard equivalent size of audio
    processDataEncode();

    // change Payload type for DTMF payload
    queue_.setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) getDtmfPayloadType(), 8000));

    // Set marker in case this is a new Event
    if (dtmf.newevent)
        queue_.setMark(true);
    queue_.sendImmediate(timestamp_, (const unsigned char *) (& (dtmf.payload)), sizeof (ost::RTPPacket::RFC2833Payload));

    // This is no longer a new event
    if (dtmf.newevent) {
        dtmf.newevent = false;
        queue_.setMark(false);
    }

    // restore the payload to audio
    const ost::StaticPayloadFormat pf(static_cast<ost::StaticPayloadType>(getCodecPayloadType()));
    queue_.setPayloadFormat(pf);

    // decrease length remaining to process for this event
    dtmf.length -= increment;
    dtmf.payload.duration++;
    // next packet is going to be the last one
    if ((dtmf.length - increment) < increment)
        dtmf.payload.ebit = true;
    if (dtmf.length < increment)
        audioRtpRecord_.dtmfQueue_.pop_front();
}


void AudioRtpSession::receiveSpeakerData()
{
    const ost::AppDataUnit* adu = queue_.getData(queue_.getFirstTimestamp());

    if (!adu)
        return;

    unsigned char* spkrDataIn = (unsigned char*) adu->getData(); // data in char
    size_t size = adu->getSize(); // size in char

    // DTMF over RTP, size must be over 4 in order to process it as voice data
    if (size > 4)
        processDataDecode(spkrDataIn, size, adu->getType());

    delete adu;
}


void AudioRtpSession::sendMicData()
{
    int compSize = processDataEncode();

    // if no data return
    if (compSize == 0)
        return;

    // Increment timestamp for outgoing packet
    timestamp_ += timestampIncrement_;

    // putData puts the data on RTP queue, sendImmediate bypass this queue
    queue_.sendImmediate(timestamp_, getMicDataEncoded(), compSize);
}


void AudioRtpSession::setSessionTimeouts()
{
    DEBUG("Set session scheduling timeout (%d) and expireTimeout (%d)",
            sfl::schedulingTimeout, sfl::expireTimeout);

    queue_.setSchedulingTimeout(sfl::schedulingTimeout);
    queue_.setExpireTimeout(sfl::expireTimeout);
}

void AudioRtpSession::setDestinationIpAddress()
{
    // Store remote ip in case we would need to forget current destination
    remote_ip_ = ost::InetHostAddress(call_.getLocalSDP()->getRemoteIP().c_str());

    if (!remote_ip_) {
        WARN("Target IP address (%s) is not correct!",
              call_.getLocalSDP()->getRemoteIP().data());
        return;
    }

    // Store remote port in case we would need to forget current destination
    remote_port_ = (unsigned short) call_.getLocalSDP()->getRemoteAudioPort();

    DEBUG("New remote address for session: %s:%d",
          call_.getLocalSDP()->getRemoteIP().data(), remote_port_);

    if (!queue_.addDestination(remote_ip_, remote_port_)) {
        WARN("Can't add new destination to session!");
        return;
    }
}

void AudioRtpSession::updateDestinationIpAddress()
{
    DEBUG("Update destination ip address");

    // Destination address are stored in a list in ccrtp
    // This method remove the current destination entry

    if (!queue_.forgetDestination(remote_ip_, remote_port_, remote_port_ + 1))
        DEBUG("Did not remove previous destination");

    // new destination is stored in call
    // we just need to recall this method
    setDestinationIpAddress();
}


int AudioRtpSession::startRtpThread(AudioCodec &audiocodec)
{
    if (isStarted_)
        return 0;

    DEBUG("Starting main thread");

    isStarted_ = true;
    setSessionTimeouts();
    setSessionMedia(audiocodec);
    initBuffers();
    initNoiseSuppress();

    queue_.enableStack();
    thread_.start();
    return 0;
}


bool AudioRtpSession::onRTPPacketRecv(ost::IncomingRTPPkt&)
{
    receiveSpeakerData();
    return true;
}

int AudioRtpSession::getIncrementForDTMF() const
{
    return timestampIncrement_;
}

}
