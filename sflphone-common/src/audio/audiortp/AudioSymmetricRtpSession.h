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
#ifndef __SFL_AUDIO_SYMMETRIC_RTP_SESSION_H__
#define __SFL_AUDIO_SYMMETRIC_RTP_SESSION_H__

#include <iostream>
#include <exception>
#include <list>
#include <cstddef>

#include "global.h"

#include "AudioRtpSession.h"
#include "AudioRtpRecordHandler.h"
#include "sip/sipcall.h"
#include "audio/codecs/audiocodec.h"

using std::ptrdiff_t;
#include <ccrtp/rtp.h>
#include <ccrtp/iqueue.h>
#include <cc++/numbers.h> // ost::Time

#include <fstream>
namespace sfl
{

// class AudioSymmetricRtpSession : protected ost::Thread, public ost::TimerPort, public AudioRtpRecordHandler, public ost::TRTPSessionBase<ost::DualRTPUDPIPv4Channel,ost::DualRTPUDPIPv4Channel,ost::AVPQueue>
class AudioSymmetricRtpSession : public AudioRtpSession, public ost::TimerPort, public ost::SymmetricRTPSession
{
    public:
        /**
        * Constructor
        * @param sipcall The pointer on the SIP call
        */
        AudioSymmetricRtpSession (SIPCall* sipcall);

        ~AudioSymmetricRtpSession();

        virtual void final ();

        // Thread associated method
        // virtual void run ();

        virtual bool onRTPPacketRecv (ost::IncomingRTPPkt& pkt) { return AudioRtpSession::onRTPPacketRecv(pkt); }

        int startSymmetricRtpThread (void) {
            return _rtpThread->start();
        }

        void stopSymmetricRtpThread (void) {
            _rtpThread->running = false;
        }

    private:

        class AudioRtpThread : public ost::Thread, public ost::TimerPort
        {
            public:
                AudioRtpThread (AudioSymmetricRtpSession *session);
                ~AudioRtpThread();

                virtual void run();

                bool running;

            private:
                AudioSymmetricRtpSession *rtpSession;
        };
        SpeexEchoCancel echoCanceller;

    protected:

        AudioRtpThread *_rtpThread;

    public:

        friend class AudioRtpThread;
};

}
#endif // __AUDIO_SYMMETRIC_RTP_SESSION_H__

