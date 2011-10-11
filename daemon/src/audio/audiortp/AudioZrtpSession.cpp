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
#include "AudioZrtpSession.h"
#include "ZrtpSessionCallback.h"

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
    _zidFilename(zidFilename)
{
    _debug("AudioZrtpSession initialized");
    initializeZid();

    setCancel(cancelDefault);

    _info("AudioZrtpSession: Setting new RTP session with destination %s:%d", _ca->getLocalIp().c_str(), _ca->getLocalAudioPort());
}

AudioZrtpSession::~AudioZrtpSession()
{
    _debug("AudioZrtpSession: Delete AudioSymmetricRtpSession instance");

    try {
        terminate();
    } catch (...) {
        _debug("AudioZrtpSession: Thread destructor didn't terminate correctly");
        throw;
    }

    Manager::instance().getMainBuffer()->unBindAll(_ca->getCallId());
}

void AudioZrtpSession::final()
{
    delete this;
}

void AudioZrtpSession::initializeZid(void)
{

    if (_zidFilename.empty()) {
        throw ZrtpZidException("zid filename empty");
    }

    std::string zidCompleteFilename;

    // xdg_config = std::string (HOMEDIR) + DIR_SEPARATOR_STR + ".cache/sflphone";

    std::string xdg_config = std::string(HOMEDIR) + DIR_SEPARATOR_STR + ".cache" + DIR_SEPARATOR_STR + PACKAGE + "/" + _zidFilename;

    _debug("    xdg_config %s", xdg_config.c_str());

    if (XDG_CACHE_HOME != NULL) {
        std::string xdg_env = std::string(XDG_CACHE_HOME) + _zidFilename;
        _debug("    xdg_env %s", xdg_env.c_str());
        (xdg_env.length() > 0) ? zidCompleteFilename = xdg_env : zidCompleteFilename = xdg_config;
    } else
        zidCompleteFilename = xdg_config;


    if (initialize(zidCompleteFilename.c_str()) >= 0) {
        _debug("Register callbacks");
        setEnableZrtp(true);
        setUserCallback(new ZrtpSessionCallback(_ca));
        return;
    }

    _debug("Initialization from ZID file failed. Trying to remove...");

    if (remove(zidCompleteFilename.c_str()) !=0) {
        _debug("Failed to remove zid file: %m");
        throw ZrtpZidException("zid file deletion failed");
    }

    if (initialize(zidCompleteFilename.c_str()) < 0) {
        _debug("ZRTP initialization failed");
        throw ZrtpZidException("zid initialization failed");
    }

    return;
}

void AudioZrtpSession::run()
{

    // Set recording sampling rate
    _ca->setRecordingSmplRate(getCodecSampleRate());

    _debug("AudioZrtpSession: Entering mainloop for call %s",_ca->getCallId().c_str());

    uint32 timeout = 0;

    while (isActive()) {

        if (timeout < 1000) {  // !(timeout/1000)
            timeout = getSchedulingTimeout();
        }

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

                if (isActive()) { // take in only if active
                    takeInDataPacket();
                }

                setCancel(cancelImmediate);
            }

            timeout = 0;
        }

    }

    _debug("AudioZrtpSession: Left main loop for call %s", _ca->getCallId().c_str());
}

}
