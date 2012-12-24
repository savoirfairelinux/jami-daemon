/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "audio_zrtp_session.h"
#include "zrtp_session_callback.h"
#include "sip/sipcall.h"
#include "logger.h"
#include "manager.h"
#include "fileutils.h"

#include <libzrtpcpp/zrtpccrtp.h>
#include <libzrtpcpp/ZrtpQueue.h>
#include <libzrtpcpp/ZrtpUserCallback.h>
#include <ccrtp/rtp.h>

namespace sfl {

AudioZrtpSession::AudioZrtpSession(SIPCall &call, const std::string &zidFilename) :
    ost::SymmetricZRTPSession(ost::InetHostAddress(call.getLocalIp().c_str()), call.getLocalAudioPort())
    , AudioRtpSession(call, *this)
    , zidFilename_(zidFilename)
    , rtpSendThread_(*this)
{
    initializeZid();
    DEBUG("Setting new RTP session with destination %s:%d",
          call_.getLocalIp().c_str(), call_.getLocalAudioPort());
    audioRtpRecord_.callId_ = call_.getCallId();
}

void AudioZrtpSession::initializeZid()
{
    if (zidFilename_.empty())
        throw ZrtpZidException("zid filename empty");

    std::string zidCompleteFilename;

    std::string xdg_config = std::string(HOMEDIR) + DIR_SEPARATOR_STR + ".cache" + DIR_SEPARATOR_STR + PACKAGE + "/" + zidFilename_;

    DEBUG("xdg_config %s", xdg_config.c_str());

    if (XDG_CACHE_HOME != NULL) {
        std::string xdg_env = std::string(XDG_CACHE_HOME) + zidFilename_;
        DEBUG("xdg_env %s", xdg_env.c_str());
        (xdg_env.length() > 0) ? zidCompleteFilename = xdg_env : zidCompleteFilename = xdg_config;
    } else
        zidCompleteFilename = xdg_config;


    if (initialize(zidCompleteFilename.c_str()) >= 0) {
        setEnableZrtp(true);
        setUserCallback(new ZrtpSessionCallback(call_));
        return;
    }

    DEBUG("Initialization from ZID file failed. Trying to remove...");

    if (remove(zidCompleteFilename.c_str()) != 0)
        throw ZrtpZidException("zid file deletion failed");

    if (initialize(zidCompleteFilename.c_str()) < 0)
        throw ZrtpZidException("zid initialization failed");

    return;
}

void AudioZrtpSession::sendMicData()
{
    int compSize = processDataEncode();

    // if no data return
    if (compSize == 0)
        return;

    // Increment timestamp for outgoing packet
    timestamp_ += timestampIncrement_;

    // this step is only needed for ZRTP
    queue_.putData(timestamp_, getMicDataEncoded(), compSize);

    // putData puts the data on RTP queue, sendImmediate bypasses this queue
    queue_.sendImmediate(timestamp_, getMicDataEncoded(), compSize);
}

AudioZrtpSession::AudioZrtpSendThread::AudioZrtpSendThread(AudioZrtpSession &session) :
    running_(true), zrtpSession_(session), thread_(0)
{}

AudioZrtpSession::AudioZrtpSendThread::~AudioZrtpSendThread()
{
    running_ = false;
    if (thread_)
        pthread_join(thread_, NULL);
}

void
AudioZrtpSession::AudioZrtpSendThread::start()
{
    pthread_create(&thread_, NULL, &runCallback, this);
}

void *
AudioZrtpSession::AudioZrtpSendThread::runCallback(void *data)
{
    AudioZrtpSession::AudioZrtpSendThread *context = static_cast<AudioZrtpSession::AudioZrtpSendThread*>(data);
    context->run();
    return NULL;
}

void AudioZrtpSession::AudioZrtpSendThread::run()
{
    DEBUG("Entering Audio zrtp thread main loop %s", running_ ? "running" : "not running");

    TimerPort::setTimer(zrtpSession_.transportRate_);

    while (running_) {
        // Send session
        if (zrtpSession_.hasDTMFPending())
            zrtpSession_.sendDtmfEvent();
        else
            zrtpSession_.sendMicData();

        Thread::sleep(TimerPort::getTimer());

        TimerPort::incTimer(zrtpSession_.transportRate_);
    }

    DEBUG("Leaving audio rtp thread loop");
}

int AudioZrtpSession::getIncrementForDTMF() const
{
    return 160;
}

void AudioZrtpSession::startReceiveThread()
{
    ost::SymmetricZRTPSession::start();
}

void AudioZrtpSession::startSendThread()
{
    rtpSendThread_.start();
}

}
