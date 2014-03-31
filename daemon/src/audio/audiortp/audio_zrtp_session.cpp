/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

namespace sfl {

AudioZrtpSession::AudioZrtpSession(SIPCall &call, const std::string &zidFilename,
                                   const std::string &localIP) :
    ost::SymmetricZRTPSession(ost::InetHostAddress(localIP.c_str()), call.getLocalAudioPort())
    , AudioRtpSession(call, *this)
    , zidFilename_(zidFilename)
#if HAVE_ZRTP_CONFIGURE
    , zrtpConfigure_()
{
    setEnrollmentMode(false);
    setSignSas(false);
#else
{
#endif
    initializeZid();
    DEBUG("Setting new RTP session with destination %s:%d",
          localIP.c_str(), call_.getLocalAudioPort());
}

std::vector<long>
AudioZrtpSession::getSocketDescriptors() const
{
    std::vector<long> result;
    result.push_back(dso->getRecvSocket());
    result.push_back(cso->getRecvSocket());
    return result;
}

void AudioZrtpSession::initializeZid()
{
    if (zidFilename_.empty())
        throw ZrtpZidException("zid filename empty");

    const std::string zidDirName(fileutils::get_cache_dir());
    const std::string zidCompleteFilename(zidDirName + DIR_SEPARATOR_STR + zidFilename_);

    fileutils::check_dir(zidDirName.c_str());

#if HAVE_ZRTP_CONFIGURE
    // workaround buggy libzrtpcpp that can't parse EC25, see:
    // https://projects.savoirfairelinux.com/issues/40216
    zrtpConfigure_.reset(new ZrtpConfigure);
    // FIXME: perhaps this should be only done for libzrtpcpp < 3?
    zrtpConfigure_->setMandatoryOnly();

    if (initialize(zidCompleteFilename.c_str(), true, zrtpConfigure_.get()) >= 0) {
        setEnableZrtp(true);
        setUserCallback(new ZrtpSessionCallback(call_));
        return;
    }
#else
    if (initialize(zidCompleteFilename.c_str()) >= 0) {
        setEnableZrtp(true);
        setUserCallback(new ZrtpSessionCallback(call_));
        return;
    }
#endif

    DEBUG("Initialization from ZID file failed. Trying to remove...");

    if (remove(zidCompleteFilename.c_str()) != 0)
        throw ZrtpZidException("zid file deletion failed");

    if (initialize(zidCompleteFilename.c_str()) < 0)
        throw ZrtpZidException("zid initialization failed");
}

int AudioZrtpSession::getIncrementForDTMF() const
{
    return 160;
}

void AudioZrtpSession::startRTPLoop()
{
    ost::SymmetricZRTPSession::startRunning();
}

}
