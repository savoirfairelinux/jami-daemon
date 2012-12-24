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
    , rtpSendThread_(*this)
{
    DEBUG("Setting new RTP session with destination %s:%d",
            call_.getLocalIp().c_str(), call_.getLocalAudioPort());
    audioRtpRecord_.callId_ = call_.getCallId();
}

AudioSymmetricRtpSession::AudioRtpSendThread::AudioRtpSendThread(AudioSymmetricRtpSession &session) :
    running_(false), rtpSession_(session), thread_(0)
{}

AudioSymmetricRtpSession::AudioRtpSendThread::~AudioRtpSendThread()
{
    running_ = false;
    if (thread_)
        pthread_join(thread_, NULL);
}

void AudioSymmetricRtpSession::AudioRtpSendThread::start()
{
    running_ = true;
    pthread_create(&thread_, NULL, &runCallback, this);
}

void *
AudioSymmetricRtpSession::AudioRtpSendThread::runCallback(void *data)
{
    AudioSymmetricRtpSession::AudioRtpSendThread *context = static_cast<AudioSymmetricRtpSession::AudioRtpSendThread*>(data);
    context->run();
    return NULL;
}

void AudioSymmetricRtpSession::AudioRtpSendThread::run()
{
    ost::TimerPort::setTimer(rtpSession_.transportRate_);
    const int MS_TO_USEC = 1000;

    while (running_) {
        // Send session
        if (rtpSession_.hasDTMFPending())
            rtpSession_.sendDtmfEvent();
        else
            rtpSession_.sendMicData();

        usleep(ost::TimerPort::getTimer() * MS_TO_USEC);

        ost::TimerPort::incTimer(rtpSession_.transportRate_);
    }
}

void AudioSymmetricRtpSession::startReceiveThread()
{
    ost::SymmetricRTPSession::start();
}

void AudioSymmetricRtpSession::startSendThread()
{
    rtpSendThread_.start();
}
}
