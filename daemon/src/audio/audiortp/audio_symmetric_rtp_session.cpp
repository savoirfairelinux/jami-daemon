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
void AudioSymmetricRtpSession::onGotSR(ost::SyncSource& source, ost::RTCPCompoundHandler::SendReport& SR, uint8 blocks)
{
    ost::SymmetricRTPSession::onGotSR(source, SR, blocks);
    // TODO: do something with this data
}

}
