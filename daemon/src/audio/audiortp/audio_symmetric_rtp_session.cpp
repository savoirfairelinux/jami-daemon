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
#include "audio_symmetric_rtp_session.h"
#include "audio_rtp_record_handler.h"
#include "sip/sdp.h"
#include "audio/audiolayer.h"

namespace sfl {

AudioSymmetricRtpSession::AudioSymmetricRtpSession(SIPCall * sipcall) :
    ost::SymmetricRTPSession(ost::InetHostAddress(sipcall->getLocalIp().c_str()), sipcall->getLocalAudioPort())
    , AudioRtpSession(sipcall, Symmetric, this, this)
    , _rtpThread(new AudioRtpThread(this))
{
    _info("AudioSymmetricRtpSession: Setting new RTP session with destination %s:%d", _ca->getLocalIp().c_str(), _ca->getLocalAudioPort());

    _audioRtpRecord._callId = _ca->getCallId();
}

AudioSymmetricRtpSession::~AudioSymmetricRtpSession()
{
    _info("AudioSymmetricRtpSession: Delete AudioSymmetricRtpSession instance");

    _rtpThread->running = false;
    delete _rtpThread;
}

AudioSymmetricRtpSession::AudioRtpThread::AudioRtpThread(AudioSymmetricRtpSession *session) : running(true), rtpSession(session)
{
    _debug("AudioSymmetricRtpSession: Create new rtp thread");
}

AudioSymmetricRtpSession::AudioRtpThread::~AudioRtpThread()
{
    _debug("AudioSymmetricRtpSession: Delete rtp thread");
}

void AudioSymmetricRtpSession::AudioRtpThread::run()
{
    int threadSleep = 20;

    TimerPort::setTimer(threadSleep);

    _debug("AudioRtpThread: Entering Audio rtp thread main loop");

    while (running) {

        // Send session
        if (rtpSession->DtmfPending())
            rtpSession->sendDtmfEvent();
        else
            rtpSession->sendMicData();

        Thread::sleep(TimerPort::getTimer());

        TimerPort::incTimer(threadSleep);
    }

    _debug("AudioRtpThread: Leaving audio rtp thread loop");
}

}
