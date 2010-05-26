/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#include "AudioZrtpSession.h"
#include "ZrtpSessionCallback.h"
#include "user_cfg.h"

#include "sip/sipcall.h"

#include <libzrtpcpp/ZrtpQueue.h>
#include <libzrtpcpp/ZrtpUserCallback.h>

#include <cstdio>
#include <cstring>
#include <cerrno>

namespace sfl
{

AudioZrtpSession::AudioZrtpSession (ManagerImpl * manager, SIPCall * sipcall, const std::string& zidFilename) :
        ost::SymmetricZRTPSession (ost::InetHostAddress (sipcall->getLocalIp().c_str()), sipcall->getLocalAudioPort()),
        AudioRtpSession<AudioZrtpSession> (manager, sipcall),
        _zidFilename (zidFilename)
{
    _debug ("AudioZrtpSession initialized");
    initializeZid();
}

void AudioZrtpSession::initializeZid (void)
{

    if (_zidFilename.empty()) {
        throw ZrtpZidException();
    }

    std::string zidCompleteFilename;

    // xdg_config = std::string (HOMEDIR) + DIR_SEPARATOR_STR + ".cache/sflphone";

    std::string xdg_config = std::string (HOMEDIR) + DIR_SEPARATOR_STR + ".cache" + DIR_SEPARATOR_STR + PROGDIR + "/" + _zidFilename;

    _debug ("    xdg_config %s", xdg_config.c_str());

    if (XDG_CACHE_HOME != NULL) {
        std::string xdg_env = std::string (XDG_CACHE_HOME) + _zidFilename;
        _debug ("    xdg_env %s", xdg_env.c_str());
        (xdg_env.length() > 0) ? zidCompleteFilename = xdg_env : zidCompleteFilename = xdg_config;
    } else
        zidCompleteFilename = xdg_config;


    if (initialize (zidCompleteFilename.c_str()) >= 0) {
        _debug ("Register callbacks");
        setEnableZrtp (true);
        setUserCallback (new ZrtpSessionCallback (_ca));
        return;
    }

    _debug ("Initialization from ZID file failed. Trying to remove...");

    if (remove (zidCompleteFilename.c_str()) !=0) {
        _debug ("Failed to remove zid file because of: %s", strerror (errno));
        throw ZrtpZidException();
    }

    if (initialize (zidCompleteFilename.c_str()) < 0) {
        _debug ("ZRTP initialization failed");
        throw ZrtpZidException();
    }

    return;
}
}
