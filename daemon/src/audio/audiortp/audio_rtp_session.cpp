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
#include <ccrtp/oqueue.h>
#include "manager.h"

#include <numeric>
#include <algorithm>

namespace sfl {
AudioRtpSession::AudioRtpSession(SIPCall &call, ost::RTPDataQueue &queue) :
    isStarted_(false)
    , queue_(queue)
    , call_(call)
    , timestamp_(0)
    , timestampIncrement_(0)
    , transportRate_(20)
    , remote_ip_()
    , remote_port_(0)
    , rxLast()
    , rxLastSeqNum(0)
    , rxJitters()
    , rtpSendThread_(*this)
    , dtmfQueue_()
    , rtpStream_(call.getCallId())
    , dtmfPayloadType_(101) // same as Asterisk
{
    queue_.setTypeOfService(ost::RTPDataQueue::tosEnhanced);
}

AudioRtpSession::~AudioRtpSession()
{
    queue_.disableStack();
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

#if 0 // Compute and print RX jitter
    auto rxTime = std::chrono::high_resolution_clock::now();
    rxJitters.push_back(std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(rxTime - rxLast).count());
    rxLast = rxTime;
    static unsigned c=0;
    if(++c == 100) {
        if(rxJitters.size() > 1000)
            rxJitters.erase(rxJitters.begin(), rxJitters.begin()+(rxJitters.size()-1000));
        double jit_mean = std::accumulate(rxJitters.begin(), rxJitters.end(), 0.0)/rxJitters.size();
        double jit_sq_sum = std::inner_product(rxJitters.begin(), rxJitters.end(), rxJitters.begin(), 0.0);
        double jit_stdev = std::sqrt(jit_sq_sum / rxJitters.size() - jit_mean*jit_mean);
        DEBUG("Jitter avg: %fms std dev %fms", jit_mean, jit_stdev);
        c = 0;
    }
#endif

    int seqNumDiff = adu->getSeqNum() - rxLastSeqNum;
    if(abs(seqNumDiff) > 512) {
        // Don't perform PLC if the delay can't be concealed without major distortion.
        // Also skip PLC in case of timestamp reset.
        rxLastSeqNum += seqNumDiff;
        seqNumDiff = 1;
    } else if(seqNumDiff < 0) {
        DEBUG("Dropping out-of-order packet %d (last %d)", rxLastSeqNum+seqNumDiff, rxLastSeqNum);
        return;
    } else {
        rxLastSeqNum += seqNumDiff;
    }
    if(rxLastSeqNum && seqNumDiff > 1) {
        DEBUG("%d packets lost", seqNumDiff-1);
        for(unsigned i=0, n=seqNumDiff-1; i<n; i++)
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
    if (compSize == 0) {
        return 0;
    }

    // Increment timestamp for outgoing packet
    timestamp_ += timestampIncrement_;

    // TODO: TEST
#if 0
    // this step is only needed for ZRTP
    queue_.putData(timestamp_, rtpStream_.getMicDataEncoded(), compSize);
#else
    // putData puts the data on RTP queue, sendImmediate bypass this queue
    queue_.sendImmediate(timestamp_, rtpStream_.getMicDataEncoded(), compSize);
#endif

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

    if (!queue_.forgetDestination(remote_ip_, remote_port_))
        DEBUG("Did not remove previous destination");

    // new destination is stored in call
    // we just need to recall this method

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


void AudioRtpSession::prepareRtpReceiveThread(const std::vector<AudioCodec*> &audioCodecs)
{
    DEBUG("Preparing receiving thread");
    isStarted_ = true;
    rxLast = std::chrono::high_resolution_clock::now();
    setSessionTimeouts();
    setSessionMedia(audioCodecs);
    rtpStream_.initBuffers();
#if HAVE_SPEEXDSP
    rtpStream_.resetDSP();
#endif

    queue_.enableStack();
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
    startReceiveThread();
    // only in this class
    rtpSendThread_.start();
}

AudioRtpSession::AudioRtpSendThread::AudioRtpSendThread(AudioRtpSession &session) :
    running_(false), rtpSession_(session), thread_(), timer_()
{}

AudioRtpSession::AudioRtpSendThread::~AudioRtpSendThread()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void AudioRtpSession::AudioRtpSendThread::start()
{
    running_ = true;
    thread_ = std::thread(&AudioRtpSendThread::run, this);
}

void AudioRtpSession::AudioRtpSendThread::run()
{
    timer_.setTimer(rtpSession_.transportRate_);
    while (running_) {
        // Send session
        if (rtpSession_.hasDTMFPending())
            rtpSession_.sendDtmfEvent();
        else
            rtpSession_.sendMicData();
        auto t = timer_.getTimer();
        if(t > 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(timer_.getTimer()-1));
        timer_.incTimer(rtpSession_.transportRate_);
    }
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
