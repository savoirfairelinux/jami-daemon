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
{
    initializeZid();
    DEBUG("Setting new RTP session with destination %s:%d",
          localIP.c_str(), call_.getLocalAudioPort());
    audioRtpRecord_.callId_ = call_.getCallId();
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

    const std::string cache_home(XDG_CACHE_HOME);
    std::string zidDirName;
    std::string zidCompleteFilename;

    if (not cache_home.empty()) {
        zidDirName = cache_home;
    } else {
        zidDirName = fileutils::get_home_dir() + DIR_SEPARATOR_STR +
                              ".cache" + DIR_SEPARATOR_STR + PACKAGE;
    }

    zidCompleteFilename = zidDirName + DIR_SEPARATOR_STR + zidFilename_;

    fileutils::check_dir(zidDirName.c_str());

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

int AudioZrtpSession::getIncrementForDTMF() const
{
    return 160;
}

void AudioZrtpSession::startReceiveThread()
{
    ost::SymmetricZRTPSession::start();
}

}
