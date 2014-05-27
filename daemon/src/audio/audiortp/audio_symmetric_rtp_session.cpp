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

#include "audio_symmetric_rtp_session.h"
#include "logger.h"
#include "sip/sipcall.h"
#include "manager.h"
#include "client/callmanager.h"

namespace sfl {

AudioSymmetricRtpSession::AudioSymmetricRtpSession(SIPCall &call) :
    ost::SymmetricRTPSession(static_cast<ost::IPV4Host>(call.getLocalIp()), call.getLocalAudioPort())
    , AudioRtpSession(call, *this)
{
    DEBUG("Setting new RTP session with destination %s:%d",
          call_.getLocalIp().toString().c_str(), call_.getLocalAudioPort());
}

AudioSymmetricRtpSession::~AudioSymmetricRtpSession()
{
    // This would normally be invoked in ost::~SymmetricRTPSession() BUT
    // we must stop it here because onRTPPacketRecv() runs in the other thread
    // and uses member variables that are destroyed here
    if (ost::Thread::isRunning()) {
        disableStack();
        ost::Thread::join();
    }
}

std::vector<long>
AudioSymmetricRtpSession::getSocketDescriptors() const
{
    std::vector<long> result;
    result.push_back(dso->getRecvSocket());
    result.push_back(cso->getRecvSocket());
    return result;
}

void AudioSymmetricRtpSession::startRTPLoop()
{
    ost::SymmetricRTPSession::startRunning();
}

// redefined from QueueRTCPManager
void AudioSymmetricRtpSession::onGotRR(ost::SyncSource& source, ost::RTCPCompoundHandler::RecvReport& RR, uint8 blocks)
{
    ost::SymmetricRTPSession::onGotRR(source, RR, blocks);
#ifdef RTP_DEBUG
    DEBUG("onGotRR");
    DEBUG("Unpacking %d blocks",blocks);
    for (int i = 0; i < blocks; ++i)
    {
        DEBUG("fractionLost : %hhu", RR.blocks[i].rinfo.fractionLost);
        DEBUG("lostMSB : %hhu", RR.blocks[i].rinfo.lostMSB);
        DEBUG("lostLSW : %hu", RR.blocks[i].rinfo.lostLSW);
        DEBUG("highestSeqNum : %u", RR.blocks[i].rinfo.highestSeqNum);
        DEBUG("jitter : %u", RR.blocks[i].rinfo.jitter);
        DEBUG("lsr : %u", RR.blocks[i].rinfo.lsr);
        DEBUG("dlsr : %u", RR.blocks[i].rinfo.dlsr);
     }
#endif
}

// redefined from QueueRTCPManager
void AudioSymmetricRtpSession::onGotSR(ost::SyncSource& source, ost::RTCPCompoundHandler::SendReport& SR, uint8 blocks)
{
#ifdef RTP_DEBUG
    DEBUG("onGotSR");
    std::cout << "I got an SR RTCP report from "
            << std::hex << (int)source.getID() << "@"
            << std::dec
            << source.getNetworkAddress() << ":"
            << source.getControlTransportPort() << std::endl;
#endif

    std::map<std::string, int> stats;
    ost::SymmetricRTPSession::onGotSR(source, SR, blocks);
    ost::RTCPSenderInfo report(SR.sinfo);

    for (int i = 0; i < blocks; ++i)
    {
        // If this is the first report drop it, stats are not complete
        if(SR.blocks[i].rinfo.lsr == 0 || SR.blocks[i].rinfo.dlsr == 0)
            continue;

        ost::RTCPReceiverInfo receiver_report(SR.blocks[i].rinfo);
        /*
        How to calculate RTT (Round Trip delay)
        A : NTP timestamp
        lsr : The middle 32 bits out of 64 in the NTP timestamp (as explained in Section 4) received as part of the most
        recent RTCP sender report (SR) packet from source SSRC_n. If no SR has been received yet, the field is set to zero.
        dlsr : The delay, expressed in units of 1/65536 seconds, between receiving the last SR packet from source SSRC_n and
        sending this reception report block. If no SR packet has been received yet from SSRC_n, the DLSR field is set to zero.
        RTT = A - lsr - dlsr;
        */

        // integer portion of timestamp is in least significant 16-bits
        const uint16 timestamp = (uint16) report.getNTPTimestampInt() & 0x0000FFFF;
        // fractional portion of timestamp is in most significant 16-bits
        const uint16 timestampFrac = (uint16) report.getNTPTimestampFrac() >> 16;


        const uint16 rttMSW =  timestamp - receiver_report.getLastSRNTPTimestampInt();
        const uint16 rttLSW = timestampFrac - receiver_report.getLastSRNTPTimestampFrac();

        uint32 rtt = rttMSW;
        rtt = rtt << 16 | rttLSW;
        rtt -= receiver_report.getDelayLastSR();
        stats["PACKET_COUNT"] = report.getPacketCount();
        stats["PACKET_LOSS"] = receiver_report.getFractionLost();
        stats["CUMUL_PACKET_LOSS"] = receiver_report.getCumulativePacketLost();
        stats["RTT"] = rtt;
        stats["LATENCY"] = 0; //TODO
        stats["HIGH_SEC_NUM"] = receiver_report.getExtendedSeqNum();
        stats["JITTER"] = receiver_report.getJitter();
        stats["DLSR"] = receiver_report.getDelayLastSR();

#ifdef RTP_DEBUG
        DEBUG("lastSR NTPTimestamp : %lu", receiver_report.getLastSRNTPTimestampFrac() << 16);
        DEBUG("NTPTimestampFrac : %lu", timestampFrac);
        DEBUG("rttMSW : %u", rttMSW);
        DEBUG("rttLSW : %u", rttLSW);
        DEBUG("RTT recomposed: %lu", rtt);
        DEBUG("LDSR: %lu", receiver_report.getDelayLastSR());
        DEBUG("Packet count : %u", stats["PACKET_COUNT"]);
        DEBUG("Fraction packet loss : %.2f", (double) stats["PACKET_LOSS"] * 100 / 256);
        DEBUG("Cumulative packet loss : %d", stats["CUMUL_PACKET_LOSS"]);
        DEBUG("HighestSeqNum : %u", stats["HIGH_SEC_NUM"]);
        DEBUG("Jitter : %u", stats["JITTER"]);
        DEBUG("RTT : %.2f", (double) stats["RTT"] / 65536);
        DEBUG("Delay since last report %.2f seconds", (double) stats["DLSR"] / 65536.0);
#endif
        Manager::instance().getClient()->getCallManager()->onRtcpReportReceived(call_.getCallId(), stats);
    }
}

#if HAVE_IPV6

AudioSymmetricRtpSessionIPv6::AudioSymmetricRtpSessionIPv6(SIPCall &call) :
    ost::SymmetricRTPSessionIPV6(static_cast<ost::IPV6Host>(call.getLocalIp()), call.getLocalAudioPort())
    , AudioRtpSession(call, *this)
{
    DEBUG("Setting new RTP/IPv6 session with destination %s:%d",
          call_.getLocalIp().toString().c_str(), call_.getLocalAudioPort());
}

AudioSymmetricRtpSessionIPv6::~AudioSymmetricRtpSessionIPv6()
{
    // see explanation in AudioSymmetricRtpSessionIPv6()
    if (ost::Thread::isRunning()) {
        disableStack();
        ost::Thread::join();
    }
}

std::vector<long>
AudioSymmetricRtpSessionIPv6::getSocketDescriptors() const
{
    std::vector<long> result;
    result.push_back(dso->getRecvSocket());
    result.push_back(cso->getRecvSocket());
    return result;
}

void AudioSymmetricRtpSessionIPv6::startRTPLoop()
{
    ost::SymmetricRTPSessionIPV6::startRunning();
}

// redefined from QueueRTCPManager
void AudioSymmetricRtpSessionIPv6::onGotRR(ost::SyncSource& source, ost::RTCPCompoundHandler::RecvReport& RR, uint8 blocks)
{
    ost::SymmetricRTPSessionIPV6::onGotRR(source, RR, blocks);
    // TODO: do something with this data
#if 0
    std::cout << "I got an RR RTCP report from "
        << std::hex << (int)source.getID() << "@"
        << std::dec
        << source.getNetworkAddress() << ":"
        << source.getControlTransportPort() << std::endl;
#endif
}

// redefined from QueueRTCPManager
void AudioSymmetricRtpSessionIPv6::onGotSR(ost::SyncSource& source, ost::RTCPCompoundHandler::SendReport& SR, uint8 blocks)
{
    ost::SymmetricRTPSessionIPV6::onGotSR(source, SR, blocks);
    // TODO: do something with this data
}

size_t
AudioSymmetricRtpSessionIPv6::recvData(unsigned char* buffer, size_t len, ost::IPV4Host&, ost::tpport_t& port)
{
    ost::IPV6Host hostv6 = call_.getLocalIp();
    ERROR("recvData %d ", hostv6.getAddressCount());
    size_t r = ost::SymmetricRTPSessionIPV6::recvData(buffer, len, hostv6, port);
    ERROR("recvData from %s %d called in ipv6 stack, size %d", IpAddr(hostv6.getAddress()).toString().c_str(), port, len);
    return r;
}

size_t
AudioSymmetricRtpSessionIPv6::recvControl(unsigned char* buffer, size_t len, ost::IPV4Host&, ost::tpport_t& port)
{
    ost::IPV6Host hostv6 = call_.getLocalIp();
    size_t r = ost::SymmetricRTPSessionIPV6::recvControl(buffer, len, hostv6, port);
    ERROR("recvControl from %s %d called in ipv6 stack, size %d", IpAddr(hostv6.getAddress()).toString().c_str(), port, len);
    return r;
}


#endif // HAVE_IPV6

}
