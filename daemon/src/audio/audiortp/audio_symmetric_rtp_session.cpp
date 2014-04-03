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
    ost::SymmetricRTPSession(ost::InetHostAddress(call.getLocalIp().c_str()), call.getLocalAudioPort())
    , AudioRtpSession(call, *this)
{
    DEBUG("Setting new RTP session with destination %s:%d",
          call_.getLocalIp().c_str(), call_.getLocalAudioPort());
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


    DEBUG("NTPMSW : %u", SR.sinfo.NTPMSW);
    DEBUG("NTPLSW : %u", SR.sinfo.NTPLSW);
    DEBUG("RTPTimestamp : %u", SR.sinfo.RTPTimestamp);
    DEBUG("packetCount : %u", SR.sinfo.packetCount);
    DEBUG("octetCount : %u", SR.sinfo.octetCount);
#endif

    std::map<std::string, int> stats;
    ost::SymmetricRTPSession::onGotSR(source, SR, blocks);
    //DEBUG("Unpacking %d blocks",blocks);
    for (int i = 0; i < blocks; ++i)
    {
        //DEBUG("fractionLostBase10 : %f", (float)SR.blocks[i].rinfo.fractionLost * 100 / 256);

        uint32 cumulativePacketLoss = SR.blocks[i].rinfo.lostMSB << 16 | SR.blocks[i].rinfo.lostLSW;

        /*
        How to calculate RTT (Round Trip delay)
        A : Reassemble NTP timestamp on 64 bits
        lsr : The middle 32 bits out of 64 in the NTP timestamp (as explained in Section 4) received as part of the most
        recent RTCP sender report (SR) packet from source SSRC_n. If no SR has been received yet, the field is set to zero.
        dlsr : The delay, expressed in units of 1/65536 seconds, between receiving the last SR packet from source SSRC_n and
        sending this reception report block. If no SR packet has been received yet from SSRC_n, the DLSR field is set to zero.
        RTT = A - lsr - dlsr;
        */

        uint64 ntpTimeStamp = SR.sinfo.NTPMSW << 32 | SR.sinfo.NTPLSW;
        uint32 rtt = SR.sinfo.NTPMSW - SR.blocks[i].rinfo.lsr - SR.blocks[i].rinfo.dlsr;

        stats["PACKET_LOSS"] = (float) SR.blocks[i].rinfo.fractionLost * 100 / 256;
        stats["CUMUL_PACKET_LOSS"] = cumulativePacketLoss;
        stats["RTT"] = rtt;
        stats["LATENCY"] = 0; //TODO

#ifdef RTP_DEBUG
        DEBUG("Cumulative packet loss : %d", cumulativePacketLoss);
        DEBUG("highestSeqNum : %u", SR.blocks[i].rinfo.highestSeqNum);
        DEBUG("jitter : %u", SR.blocks[i].rinfo.jitter);
        DEBUG("RTT : %u", rtt);
        DEBUG("lsr : %u", SR.blocks[i].rinfo.lsr);
        DEBUG("dlsr : %u", SR.blocks[i].rinfo.dlsr);
#endif
    }

    // We send the report only once but that should be for each RR blocks (each participant) //TODO
    Manager::instance().getClient()->getCallManager()->onRtcpReportReceived(call_.getCallId(), stats);
}

}
