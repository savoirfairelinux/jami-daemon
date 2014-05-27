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
#ifndef AUDIO_SYMMETRIC_RTP_SESSION_H_
#define AUDIO_SYMMETRIC_RTP_SESSION_H_

#include "global.h"
#include "audio_rtp_session.h"
#include "ip_utils.h"
#include "noncopyable.h"

#pragma GCC diagnostic ignored "-Weffc++"
#include <ccrtp/rtp.h>
#include <ccrtp/iqueue.h>

#include <exception>
#include <cassert>
#include <cstddef>

class SIPCall;

namespace sfl {

class AudioSymmetricRtpSession : public ost::SymmetricRTPSession, public AudioRtpSession {
    public:
        /**
        * Constructor
        * @param call The SIP call
        */
        AudioSymmetricRtpSession(SIPCall &call);
        ~AudioSymmetricRtpSession();

        std::vector<long>
        getSocketDescriptors() const;

        virtual bool onRTPPacketRecv(ost::IncomingRTPPkt& pkt) {
            return AudioRtpSession::onRTPPacketRecv(pkt);
        }

    private:
        void onGotRR(ost::SyncSource& source, ost::RTCPCompoundHandler::RecvReport& RR, uint8 blocks);
        void onGotSR(ost::SyncSource& source, ost::RTCPCompoundHandler::SendReport& SR, uint8 blocks);

        NON_COPYABLE(AudioSymmetricRtpSession);

        void startRTPLoop();
};

#if HAVE_IPV6

class AudioSymmetricRtpSessionIPv6 : public ost::SymmetricRTPSessionIPV6, public AudioRtpSession {
    public:
        /**
        * Constructor
        * @param call The SIP call
        */
        AudioSymmetricRtpSessionIPv6(SIPCall &call);
        ~AudioSymmetricRtpSessionIPv6();

        std::vector<long>
        getSocketDescriptors() const;

        virtual bool onRTPPacketRecv(ost::IncomingRTPPkt& pkt) {
            return AudioRtpSession::onRTPPacketRecv(pkt);
        }

    protected:
        virtual size_t recvData(unsigned char* buffer, size_t len, ost::IPV4Host&, ost::tpport_t& port);
        virtual size_t recvControl(unsigned char* buffer, size_t len, ost::IPV4Host&, ost::tpport_t& port);

    private:
        void onGotRR(ost::SyncSource& source, ost::RTCPCompoundHandler::RecvReport& RR, uint8 blocks);
        void onGotSR(ost::SyncSource& source, ost::RTCPCompoundHandler::SendReport& SR, uint8 blocks);

        NON_COPYABLE(AudioSymmetricRtpSessionIPv6);

        void startRTPLoop();
};

#endif // HAVE_IPV6

}
#pragma GCC diagnostic warning "-Weffc++"
#endif // AUDIO_SYMMETRIC_RTP_SESSION_H__

