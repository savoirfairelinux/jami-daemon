/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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
#include "AudioZrtpSession.h"
#include "ZrtpSessionCallback.h"

#include "sip/sipcall.h"
#include "sip/sdp.h"
#include "audio/audiolayer.h"
#include "manager.h"

#include <libzrtpcpp/zrtpccrtp.h>
#include <libzrtpcpp/ZrtpQueue.h>
#include <libzrtpcpp/ZrtpUserCallback.h>

#include <cstdio>
#include <cstring>
#include <cerrno>

#include <ccrtp/rtp.h>

namespace sfl
{

AudioZrtpSession::AudioZrtpSession (SIPCall * sipcall, const std::string& zidFilename) :
    // ost::SymmetricZRTPSession (ost::InetHostAddress (sipcall->getLocalIp().c_str()), sipcall->getLocalAudioPort()),
    AudioRtpRecordHandler (sipcall),
    ost::TRTPSessionBase<ost::SymmetricRTPChannel, ost::SymmetricRTPChannel, ost::ZrtpQueue> (ost::InetHostAddress (sipcall->getLocalIp().c_str()),
            sipcall->getLocalAudioPort(),
            0,
            ost::MembershipBookkeeping::defaultMembersHashSize,
            ost::defaultApplication())
    , _zidFilename (zidFilename)
    , _mainloopSemaphore (0)
    , _timestamp (0)
    , _timestampIncrement (0)
    , _timestampCount (0)
    , _ca (sipcall)
{
    _debug ("AudioZrtpSession initialized");
    initializeZid();

    setCancel (cancelDefault);

    assert (_ca);

    _info ("AudioZrtpSession: Setting new RTP session with destination %s:%d", _ca->getLocalIp().c_str(), _ca->getLocalAudioPort());

    setTypeOfService (tosEnhanced);
}

AudioZrtpSession::~AudioZrtpSession()
{
    _debug ("AudioZrtpSession: Delete AudioRtpSession instance");

    try {
        terminate();
    } catch (...) {
        _debugException ("AudioZrtpSession: Thread destructor didn't terminate correctly");
        throw;
    }

    Manager::instance().getMainBuffer()->unBindAll (_ca->getCallId());
}


void AudioZrtpSession::final()
{
    delete this;
}



void AudioZrtpSession::initializeZid (void)
{

    if (_zidFilename.empty()) {
        throw ZrtpZidException("zid filename empty");
    }

    std::string zidCompleteFilename;

    // xdg_config = std::string (HOMEDIR) + DIR_SEPARATOR_STR + ".cache/sflphone";

    std::string xdg_config = std::string (HOMEDIR) + DIR_SEPARATOR_STR + ".cache" + DIR_SEPARATOR_STR + PROGDIR + "/" + _zidFilename;

    _debug ("    xdg_config %s", xdg_config.c_str());

    if (XDG_CACHE_HOME != NULL) {
        std::string xdg_env = std::string (XDG_CACHE_HOME) + _zidFilename;
        _debug ("    xdg_env %s", xdg_env.c_str());
        (xdg_env.length() > 0) ? zidCompleteFilename = xdg_env : zidCompleteFilename = xdg_config;
    } else
        zidCompleteFilename = xdg_config;


    if (initialize (zidCompleteFilename.c_str()) >= 0) {
        _debug ("Register callbacks");
        setEnableZrtp (true);
        setUserCallback (new ZrtpSessionCallback (_ca));
        return;
    }

    _debug ("Initialization from ZID file failed. Trying to remove...");

    if (remove (zidCompleteFilename.c_str()) !=0) {
        _debug ("Failed to remove zid file because of: %s", strerror (errno));
        throw ZrtpZidException("zid file deletion failed");
    }

    if (initialize (zidCompleteFilename.c_str()) < 0) {
        _debug ("ZRTP initialization failed");
        throw ZrtpZidException("zid initialization failed");
    }

    return;
}

void AudioZrtpSession::setSessionTimeouts (void)
{
    _debug ("AudioZrtpSession: Set session scheduling timeout (%d) and expireTimeout (%d)", sfl::schedulingTimeout, sfl::expireTimeout);

    setSchedulingTimeout (sfl::schedulingTimeout);
    setExpireTimeout (sfl::expireTimeout);
}

void AudioZrtpSession::setSessionMedia (AudioCodec* audioCodec)
{
    _debug ("AudioRtpSession: Set session media");

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

    _debug ("AudioZrtpSession: Codec payload: %d", payloadType);
    _debug ("AudioZrtpSession: Codec sampling rate: %d", smplRate);
    _debug ("AudioZrtpSession: Codec frame size: %d", frameSize);
    _debug ("AudioZrtpSession: RTP timestamp increment: %d", _timestampIncrement);

    // Even if specified as a 16 kHz codec, G722 requires rtp sending rate to be 8 kHz
    if (dynamic) {
        _debug ("AudioRtpSession: Setting dynamic payload format");
        setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) payloadType, smplRate));
    } else {
        _debug ("AudioRtpSession: Setting static payload format");
        setPayloadFormat (ost::StaticPayloadFormat ( (ost::StaticPayloadType) payloadType));
    }

}

void AudioZrtpSession::updateSessionMedia (AudioCodec *audioCodec)
{
    _debug ("AudioRtpSession: Update session media");

    //
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
    _debug ("AudioRtpSession: Codec sampling rate: %d", smplRate);
    _debug ("AudioRtpSession: Codec frame size: %d", frameSize);
    _debug ("AudioRtpSession: RTP timestamp increment: %d", _timestampIncrement);

    // Even if specified as a 16 kHz codec, G722 requires rtp sending rate to be 8 kHz
    if (dynamic) {
        _debug ("AudioRtpSession: Setting dynamic payload format");
        setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) payloadType, smplRate));
    } else {
        _debug ("AudioRtpSession: Setting static payload format");
        setPayloadFormat (ost::StaticPayloadFormat ( (ost::StaticPayloadType) payloadType));
    }
}

void AudioZrtpSession::setDestinationIpAddress (void)
{
    _info ("AudioZrtpSession: Setting IP address for the RTP session");

    if (_ca == NULL) {
        _error ("AudioZrtpSession: Sipcall is gone.");
        throw AudioRtpSessionException();
    }

    // Store remote ip in case we would need to forget current destination
    _remote_ip = ost::InetHostAddress (_ca->getLocalSDP()->getRemoteIP().c_str());

    if (!_remote_ip) {
        _warn ("AudioZrtpSession: Target IP address (%s) is not correct!",
               _ca->getLocalSDP()->getRemoteIP().data());
        return;
    }

    // Store remote port in case we would need to forget current destination
    _remote_port = (unsigned short) _ca->getLocalSDP()->getRemoteAudioPort();

    _info ("AudioZrtpSession: New remote address for session: %s:%d",
           _ca->getLocalSDP()->getRemoteIP().data(), _remote_port);

    if (!addDestination (_remote_ip, _remote_port)) {
        _warn ("AudioZrtpSession: Can't add new destination to session!");
        return;
    }
}

void AudioZrtpSession::updateDestinationIpAddress (void)
{
    _debug ("AudioZrtpSession: Update destination ip address");

    // Destination address are stored in a list in ccrtp
    // This method remove the current destination entry

    if (!forgetDestination (_remote_ip, _remote_port, _remote_port+1))
        _warn ("AudioZrtpSession: Could not remove previous destination");

    // new destination is stored in call
    // we just need to recall this method
    setDestinationIpAddress();
}


void AudioZrtpSession::sendDtmfEvent (sfl::DtmfEvent *dtmf)
{
    _debug ("AudioZrtpSession: Send Dtmf %d", getEventQueueSize());

    _timestamp += 160;

    // discard equivalent size of audio
    processDataEncode();

    // change Payload type for DTMF payload
    setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) 101, 8000));

    // Set marker in case this is a new Event
    if (dtmf->newevent)
        setMark (true);

    // putData (_timestamp, (const unsigned char*) (& (dtmf->payload)), sizeof (ost::RTPPacket::RFC2833Payload));
    sendImmediate (_timestamp, (const unsigned char*) (& (dtmf->payload)), sizeof (ost::RTPPacket::RFC2833Payload));

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

bool AudioZrtpSession::onRTPPacketRecv (ost::IncomingRTPPkt&)
{
    receiveSpeakerData();

    return true;
}


void AudioZrtpSession::sendMicData()
{
    int compSize = processDataEncode();

    // if no data return
    if (!compSize)
        return;

    // Increment timestamp for outgoing packet
    _timestamp += _timestampIncrement;

    // putData put the data on RTP queue, sendImmediate bypass this queue
    putData (_timestamp, getMicDataEncoded(), compSize);
    sendImmediate (_timestamp, getMicDataEncoded(), compSize);
}


void AudioZrtpSession::receiveSpeakerData ()
{

    const ost::AppDataUnit* adu = NULL;

    int packetTimestamp = getFirstTimestamp();

    adu = getData (packetTimestamp);

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

int AudioZrtpSession::startRtpThread (AudioCodec* audiocodec)
{
    if (_isStarted)
        return 0;

    _debug ("AudioZrtpSession: Starting main thread");
    _isStarted = true;
    setSessionTimeouts();
    setSessionMedia (audiocodec);
    initBuffers();
    initNoiseSuppress();
    enableStack();
    int ret = start (_mainloopSemaphore);
    return ret;
}

void AudioZrtpSession::stopRtpThread ()
{
    _debug ("AudioZrtpSession: Stoping main thread");

    disableStack();
}

void AudioZrtpSession::run ()
{

    // Set recording sampling rate
    _ca->setRecordingSmplRate (getCodecSampleRate());

    _debug ("AudioZrtpSession: Entering mainloop for call %s",_ca->getCallId().c_str());

    uint32 timeout = 0;

    while (isActive()) {

        if (timeout < 1000) {  // !(timeout/1000)
            timeout = getSchedulingTimeout();
        }

        // Send session
        if (getEventQueueSize() > 0) {
            sendDtmfEvent (getEventQueue()->front());
        } else {
            sendMicData ();
        }

        setCancel (cancelDeferred);
        controlReceptionService();
        controlTransmissionService();
        setCancel (cancelImmediate);
        uint32 maxWait = timeval2microtimeout (getRTCPCheckInterval());
        // make sure the scheduling timeout is
        // <= the check interval for RTCP
        // packets
        timeout = (timeout > maxWait) ? maxWait : timeout;

        if (timeout < 1000) {   // !(timeout/1000)
            setCancel (cancelDeferred);
            // dispatchDataPacket();
            setCancel (cancelImmediate);
            timerTick();
        } else {
            if (isPendingData (timeout/1000)) {
                setCancel (cancelDeferred);

                if (isActive()) { // take in only if active
                    takeInDataPacket();
                }

                setCancel (cancelImmediate);
            }

            timeout = 0;
        }

    }

    _debug ("AudioZrtpSession: Left main loop for call %s", _ca->getCallId().c_str());
}

}
