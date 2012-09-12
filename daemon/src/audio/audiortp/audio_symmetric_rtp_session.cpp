/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
    ost::TimerPort()
    , ost::SymmetricRTPSession(ost::InetHostAddress(call.getLocalIp().c_str()), call.getLocalAudioPort())
    , AudioRtpSession(call, *this, *this)
    , rtpThread_(*this)
    , audiocodec_(0)
{
    DEBUG("Setting new RTP session with destination %s:%d",
            call_.getLocalIp().c_str(), call_.getLocalAudioPort());
    audioRtpRecord_.callId_ = call_.getCallId();
}

AudioSymmetricRtpSession::~AudioSymmetricRtpSession()
{
    if (rtpThread_.running_) {
        rtpThread_.running_ = false;
        rtpThread_.join();
    }
}

AudioSymmetricRtpSession::AudioRtpThread::AudioRtpThread(AudioSymmetricRtpSession &session) : running_(true), rtpSession_(session)
{}

void AudioSymmetricRtpSession::AudioRtpThread::run()
{
    TimerPort::setTimer(rtpSession_.transportRate_);

    while (running_) {
        // Send session
        if (rtpSession_.hasDTMFPending())
            rtpSession_.sendDtmfEvent();
        else
            rtpSession_.sendMicData();

        Thread::sleep(TimerPort::getTimer());

        TimerPort::incTimer(rtpSession_.transportRate_);
    }
}

int AudioSymmetricRtpSession::startRtpThread(AudioCodec &audiocodec)
{
    DEBUG("Starting main thread");
    if (isStarted_)
        return 0;

    audiocodec_ = &audiocodec;
    AudioRtpSession::startRtpThread(audiocodec);
    return startSymmetricRtpThread();
}
}
