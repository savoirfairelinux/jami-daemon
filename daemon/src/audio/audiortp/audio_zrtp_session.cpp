/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "config.h"
#include "audio_zrtp_session.h"
#include "zrtp_session_callback.h"

#include "sip/sipcall.h"
#include "sip/sdp.h"
#include "audio/audiolayer.h"
#include "manager.h"

#include <libzrtpcpp/zrtpccrtp.h>
#include <libzrtpcpp/ZrtpQueue.h>
#include <libzrtpcpp/ZrtpUserCallback.h>

#include <cstdio>
#include <cstring>
#include <cerrno>

#include <ccrtp/rtp.h>

namespace sfl {

AudioZrtpSession::AudioZrtpSession(SIPCall * sipcall, const std::string& zidFilename) :
    AudioRtpSession(sipcall, Zrtp, this, this),
    ost::TRTPSessionBase<ost::SymmetricRTPChannel, ost::SymmetricRTPChannel, ost::ZrtpQueue>(ost::InetHostAddress(sipcall->getLocalIp().c_str()),
            sipcall->getLocalAudioPort(),
            0,
            ost::MembershipBookkeeping::defaultMembersHashSize,
            ost::defaultApplication()),
    zidFilename_(zidFilename)
{
    DEBUG("AudioZrtpSession initialized");
    initializeZid();

    setCancel(cancelDefault);

    INFO("AudioZrtpSession: Setting new RTP session with destination %s:%d", ca_->getLocalIp().c_str(), ca_->getLocalAudioPort());
}

AudioZrtpSession::~AudioZrtpSession()
{
    ost::Thread::terminate();
    Manager::instance().getMainBuffer()->unBindAll(ca_->getCallId());
}

void AudioZrtpSession::final()
{
// tmatth:Oct 25 2011:FIXME:
// This was crashing...seems like it's not necessary. Double check
// with valgrind/helgrind
//    delete this;
}

void AudioZrtpSession::initializeZid()
{
    if (zidFilename_.empty())
        throw ZrtpZidException("zid filename empty");

    std::string zidCompleteFilename;

    std::string xdg_config = std::string(HOMEDIR) + DIR_SEPARATOR_STR + ".cache" + DIR_SEPARATOR_STR + PACKAGE + "/" + zidFilename_;

    DEBUG("    xdg_config %s", xdg_config.c_str());

    if (XDG_CACHE_HOME != NULL) {
        std::string xdg_env = std::string(XDG_CACHE_HOME) + zidFilename_;
        DEBUG("    xdg_env %s", xdg_env.c_str());
        (xdg_env.length() > 0) ? zidCompleteFilename = xdg_env : zidCompleteFilename = xdg_config;
    } else
        zidCompleteFilename = xdg_config;


    if (initialize(zidCompleteFilename.c_str()) >= 0) {
        DEBUG("Register callbacks");
        setEnableZrtp(true);
        setUserCallback(new ZrtpSessionCallback(ca_));
        return;
    }

    DEBUG("Initialization from ZID file failed. Trying to remove...");

    if (remove(zidCompleteFilename.c_str()) != 0)
        throw ZrtpZidException("zid file deletion failed");

    if (initialize(zidCompleteFilename.c_str()) < 0)
        throw ZrtpZidException("zid initialization failed");

    return;
}

void AudioZrtpSession::run()
{
    // Set recording sampling rate
    ca_->setRecordingSmplRate(getCodecSampleRate());
    DEBUG("AudioZrtpSession: Entering mainloop for call %s", ca_->getCallId().c_str());

    uint32 timeout = 0;

    while (isActive()) {
        if (timeout < 1000)
            timeout = getSchedulingTimeout();

        // Send session
        if (DtmfPending())
            sendDtmfEvent();
        else
            sendMicData();

        setCancel(cancelDeferred);
        controlReceptionService();
        controlTransmissionService();
        setCancel(cancelImmediate);
        uint32 maxWait = timeval2microtimeout(getRTCPCheckInterval());
        // make sure the scheduling timeout is
        // <= the check interval for RTCP
        // packets
        timeout = (timeout > maxWait) ? maxWait : timeout;

        if (timeout < 1000) {   // !(timeout/1000)
            setCancel(cancelDeferred);
            // dispatchDataPacket();
            setCancel(cancelImmediate);
            timerTick();
        } else {
            if (isPendingData(timeout/1000)) {
                setCancel(cancelDeferred);

                if (isActive())
                    takeInDataPacket();

                setCancel(cancelImmediate);
            }
            timeout = 0;
        }
    }

    DEBUG("AudioZrtpSession: Left main loop for call %s", ca_->getCallId().c_str());
}
}
