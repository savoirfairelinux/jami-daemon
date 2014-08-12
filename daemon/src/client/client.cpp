/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
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

/* Note: this file is compiled only when dbus is not available */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "client/client.h"
#include "client/callmanager.h"
#include "client/configurationmanager.h"
#include "client/presencemanager.h"

#ifdef SFL_VIDEO
#include "videomanager.h"
#endif // SFL_VIDEO

Client::Client() :
    callManager_(new CallManager)
    , configurationManager_(new ConfigurationManager)
#ifdef SFL_PRESENCE
    , presenceManager_(new PresenceManager)
#endif
#if HAVE_DBUS
    , instanceManager_(0)
    , dispatcher_(0)
#endif
#ifdef SFL_VIDEO
    , videoManager_(0)
#endif
#ifdef USE_NETWORKMANAGER
    , networkManager_(0)
#endif
{}

Client::~Client()
{
#ifdef USE_NETWORKMANAGER
    delete networkManager_;
#endif
#ifdef SFL_VIDEO
    delete videoManager_;
#endif
#if HAVE_DBUS
    delete dispatcher_;
    delete instanceManager_;
#endif
    delete configurationManager_;
#ifdef SFL_PRESENCE
    delete presenceManager_;
#endif
    delete callManager_;
}

int Client::event_loop()
{
    return 0;
}

int Client::exit()
{
    return 0;
}

CallManager * Client::getCallManager()
{
    return callManager_;
}

ConfigurationManager * Client::getConfigurationManager()
{
    return configurationManager_;
}

#ifdef SFL_PRESENCE
PresenceManager * Client::getPresenceManager()
{
    return presenceManager_;
}
#endif

#ifdef SFL_VIDEO
VideoManager * Client::getVideoManager()
{
    return videoManager_;
}
#endif
