/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "audio_rtp_session.h"
#include "logger.h"
#include "sip/sdp.h"
#include "sip/sipcall.h"

#ifdef RTP_DEBUG
#include <numeric>
#include <algorithm>
#endif

namespace sfl {
AudioRtpSession::AudioRtpSession(SIPCall &call, ost::RTPDataQueue &queue) :
    isStarted_(false)
    , queue_(queue)
    , call_(call)
    , timestamp_(0)
    , timestampIncrement_(0)
    , rtpStream_(call.getCallId())
    , transportRate_(20)
    , remoteIp_()
    , rxLastSeqNum_(0)
#ifdef RTP_DEBUG
    , rxLast_()
    , rxJitters_()
    , jitterReportInterval_(0)
#endif
    , dtmfQueue_()
    , dtmfPayloadType_(101) // same as Asterisk
    , loop_([] { return true; },
            std::bind(&AudioRtpSession::process, this),
            [] {})
{
    queue_.setTypeOfService(ost::RTPDataQueue::tosEnhanced);
}

AudioRtpSession::~AudioRtpSession()
{
    loop_.join();
}

void AudioRtpSession::updateSessionMedia(const std::vector<AudioCodec*> &audioCodecs)
{
    if (rtpStream_.codecsDiffer(audioCodecs))
        setSessionMedia(audioCodecs);
}

void AudioRtpSession::setSessionMedia(const std::vector<AudioCodec*> &audioCodecs)
{
    rtpStream_.setRtpMedia(audioCodecs);

    // G722 requires timestamp to be incremented at 8kHz
    const ost::PayloadType payloadType = rtpStream_.getEncoderPayloadType();

    if (payloadType == ost::sptG722) {
        const int G722_RTP_TIME_INCREMENT = 160;
        timestampIncrement_ = G722_RTP_TIME_INCREMENT;
    } else
        timestampIncrement_ = rtpStream_.getEncoderFrameSize();

    const AudioFormat encoderFormat(rtpStream_.getEncoderFormat());

    if (payloadType == ost::sptG722) {
        const int G722_RTP_CLOCK_RATE = 8000;
        queue_.setPayloadFormat(ost::DynamicPayloadFormat(payloadType, G722_RTP_CLOCK_RATE));
    } else {
        if (rtpStream_.hasDynamicPayload())
            queue_.setPayloadFormat(ost::DynamicPayloadFormat(payloadType, encoderFormat.sample_rate));
        else
            queue_.setPayloadFormat(ost::StaticPayloadFormat(static_cast<ost::StaticPayloadType>(payloadType)));
    }

    transportRate_ = rtpStream_.getTransportRate();
    DEBUG("Switching to a transport rate of %d ms", transportRate_);
}

void AudioRtpSession::sendDtmfEvent()
{
    DTMFEvent &dtmf(dtmfQueue_.front());
    DEBUG("Send RTP Dtmf (%d)", dtmf.payload.event);

    const int increment = getIncrementForDTMF();
    if (dtmf.newevent)
        timestamp_ += increment;

    // discard equivalent size of audio
    rtpStream_.processDataEncode();

    // change Payload type for DTMF payload
    queue_.setPayloadFormat(ost::DynamicPayloadFormat((ost::PayloadType) dtmfPayloadType_, 8000));

    // Set marker in case this is a new Event
    if (dtmf.newevent)
        queue_.setMark(true);

    // Send end packet three times (without changing it). Sequence number is
    // incremented automatically by ccrtp, which is the correct behaviour.
    const unsigned repetitions = dtmf.payload.ebit ? 3 : 1;
    for (unsigned i = 0; i < repetitions; ++i)
        queue_.sendImmediate(timestamp_, (const unsigned char *)(& (dtmf.payload)), sizeof(ost::RTPPacket::RFC2833Payload));

    // This is no longer a new event
    if (dtmf.newevent) {
        dtmf.newevent = false;
        queue_.setMark(false);
    }

    // restore the payload to audio
    const int pt = rtpStream_.getEncoderPayloadType();
    const AudioFormat encoderFormat = rtpStream_.getEncoderFormat();
    if (rtpStream_.hasDynamicPayload()) {
        const ost::DynamicPayloadFormat pf(pt, encoderFormat.sample_rate);
        queue_.setPayloadFormat(pf);
    } else {
        const ost::StaticPayloadFormat pf(static_cast<ost::StaticPayloadType>(pt));
        queue_.setPayloadFormat(pf);
    }

    // decrease length remaining to process for this event
    dtmf.length -= increment;
    dtmf.payload.duration++;

    // next packet is going to be the end packet (transmitted 3 times)
    if ((dtmf.length - increment) < increment)
        dtmf.payload.ebit = true;

    if (dtmf.length < increment)
        dtmfQueue_.pop_front();
}


void AudioRtpSession::receiveSpeakerData()
{
    const ost::AppDataUnit* adu = queue_.getData(queue_.getFirstTimestamp());

    if (!adu)
        return;

#ifdef RTP_DEBUG
    // Compute and print RX jitter
    auto rxTime = std::chrono::high_resolution_clock::now();
    rxJitters_.push_back(std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(rxTime - rxLast_).count());
    rxLast_ = rxTime;
    if (++jitterReportInterval_ == 100) {
        if (rxJitters_.size() > 1000)
            rxJitters_.erase(rxJitters_.begin(), rxJitters_.begin() + (rxJitters_.size() - 1000));
        const double jit_mean = std::accumulate(rxJitters_.begin(), rxJitters_.end(), 0.0) / rxJitters_.size();
        const double jit_sq_sum = std::inner_product(rxJitters_.begin(), rxJitters_.end(), rxJitters_.begin(), 0.0);
        const double jit_stdev = std::sqrt(jit_sq_sum / rxJitters_.size() - jit_mean * jit_mean);
        DEBUG("Jitter avg: %fms std dev %fms", jit_mean, jit_stdev);
        jitterReportInterval_ = 0;
    }
#endif

    int seqNumDiff = adu->getSeqNum() - rxLastSeqNum_;
    if (abs(seqNumDiff) > 512) {
        // Don't perform PLC if the delay can't be concealed without major distortion.
        // Also skip PLC in case of timestamp reset.
        rxLastSeqNum_ += seqNumDiff;
        seqNumDiff = 1;
    } else if (seqNumDiff < 0) {
        DEBUG("Dropping out-of-order packet %d (last %d)", rxLastSeqNum_ + seqNumDiff, rxLastSeqNum_);
        return;
    } else {
        rxLastSeqNum_ += seqNumDiff;
    }
    if (rxLastSeqNum_ && seqNumDiff > 1) {
        DEBUG("%d packets lost", seqNumDiff-1);
        for (unsigned i = 0, n = seqNumDiff - 1; i < n; i++)
            rtpStream_.processDataDecode(nullptr, 0, adu->getType());
    }

    uint8_t* spkrDataIn = (uint8_t*) adu->getData(); // data in char
    size_t size = adu->getSize(); // size in char

    // DTMF over RTP, size must be over 4 in order to process it as voice data
    if (size > 4)
        rtpStream_.processDataDecode(spkrDataIn, size, adu->getType());

    delete adu;
}


size_t AudioRtpSession::sendMicData()
{
    size_t compSize = rtpStream_.processDataEncode();

    // if no data return
    if (compSize == 0)
        return 0;

    // initialize once
    int ccrtpTimestamp = queue_.getCurrentTimestamp();
    if (std::abs(timestamp_ - ccrtpTimestamp) > timestampIncrement_/2) {
        timestamp_ = ccrtpTimestamp;
    }

    // Increment timestamp for outgoing packet
    timestamp_ += timestampIncrement_;

    queue_.putData(timestamp_, rtpStream_.getMicDataEncoded(), compSize);
    return compSize;
}


void AudioRtpSession::setSessionTimeouts()
{
    const unsigned schedulingTimeout = 4000;
    const unsigned expireTimeout = 1000000;
    DEBUG("Set session scheduling timeout (%d) and expireTimeout (%d)",
          schedulingTimeout, expireTimeout);

    queue_.setSchedulingTimeout(schedulingTimeout);
    queue_.setExpireTimeout(expireTimeout);
}

void AudioRtpSession::updateDestinationIpAddress()
{
    DEBUG("Update destination ip address");

    // Destination address are stored in a list in ccrtp
    // This method remove the current destination entry
    if (!(remoteIp_.isIpv4()  && queue_.forgetDestination(static_cast<ost::IPV4Host>(remoteIp_), remoteIp_.getPort()))
#if HAVE_IPV6
     && !(remoteIp_.isIpv6() == AF_INET6 && queue_.forgetDestination(static_cast<ost::IPV6Host>(remoteIp_), remoteIp_.getPort()))
#endif
    ) DEBUG("Did not remove previous destination");

    IpAddr remote = {call_.getLocalSDP()->getRemoteIP()};
    remote.setPort(call_.getLocalSDP()->getRemoteAudioPort());
    if (!remote) {
        WARN("Target IP address (%s) is not correct!", call_.getLocalSDP()->getRemoteIP().c_str());
        return;
    }
    remoteIp_ = remote;
    DEBUG("New remote address for session: %s", remote.toString(true).c_str());

    if (!(remoteIp_.isIpv4()  && queue_.addDestination(static_cast<ost::IPV4Host>(remoteIp_), remoteIp_.getPort()))
#if HAVE_IPV6
     && !(remoteIp_.isIpv6() && queue_.addDestination(static_cast<ost::IPV6Host>(remoteIp_), remoteIp_.getPort()))
#endif
    ) WARN("Can't add new destination to session!");
}


void AudioRtpSession::prepareRtpReceiveThread(const std::vector<AudioCodec*> &audioCodecs)
{
    DEBUG("Preparing receiving thread");
    isStarted_ = true;
#ifdef RTP_DEBUG
    rxLast_ = std::chrono::high_resolution_clock::now();
#endif
    setSessionTimeouts();
    setSessionMedia(audioCodecs);
    rtpStream_.initBuffers();
#if HAVE_SPEEXDSP
    rtpStream_.resetDSP();
#endif
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

void AudioRtpSession::startRtpThreads(const std::vector<AudioCodec*> &audioCodecs)
{
    if (isStarted_)
        return;

    prepareRtpReceiveThread(audioCodecs);
    // implemented in subclasses
    startRTPLoop();
    // only in this class
    loop_.start();
}

void AudioRtpSession::process()
{
    // Send session
    if (hasDTMFPending())
        sendDtmfEvent();
    else
        sendMicData();

    rtpStream_.waitForDataEncode(std::chrono::milliseconds(transportRate_));
}

void AudioRtpSession::putDtmfEvent(char digit)
{
    DTMFEvent dtmf(digit);
    dtmfQueue_.push_back(dtmf);
}

CachedAudioRtpState *
AudioRtpSession::saveState() const
{
    ERROR("Not implemented");
    return nullptr;
}

void
AudioRtpSession::restoreState(const CachedAudioRtpState &state UNUSED)
{
    ERROR("Not implemented");
}
}
