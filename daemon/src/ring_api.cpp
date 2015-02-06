/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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
#include <string>
#include <vector>
#include <map>
#include <cstdlib>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "manager.h"
#include "managerimpl.h"
#include "logger.h"
#include "dring.h"
#include "client/callmanager.h"
#include "client/configurationmanager.h"
#include "client/presencemanager.h"

#ifdef RING_VIDEO
#include "client/videomanager.h"
#endif // RING_VIDEO

static ring::CallManager* getCallManager()
{
    return ring::Manager::instance().getClient()->getCallManager();
}

static ring::ConfigurationManager* getConfigurationManager()
{
    return ring::Manager::instance().getClient()->getConfigurationManager();
}

static ring::PresenceManager* getPresenceManager()
{
    return ring::Manager::instance().getClient()->getPresenceManager();
}

#ifdef RING_VIDEO
static ring::VideoManager* getVideoManager()
{
    return ring::Manager::instance().getClient()->getVideoManager();
}
#endif

const char *
ring_version()
{
    return PACKAGE_VERSION;
}

int ring_init(const std::map<EventHandlerKey, void*>& ev_handlers, enum ring_init_flag flags)
{
    // Handle flags
    setDebugMode(flags & RING_FLAG_DEBUG);
    setConsoleLog(flags & RING_FLAG_CONSOLE_LOG);

    // Create manager
    try {
        // FIXME: static evil
        static ring::ManagerImpl *manager;
        // ensure that we haven't been in this function before
        assert(!manager);
        manager = &(ring::Manager::instance());
    } catch (...) {
        return -RING_ERR_MANAGER_INIT;
    }

    // Register user event handlers
    for ( const auto &entry : ev_handlers ) {
        switch(entry.first) {
            case EventHandlerKey::CALL:
            ring::Manager::instance().getCallManager()->registerEvHandlers(static_cast<ring::ring_call_ev_handlers*>(entry.second));
            break;
            case EventHandlerKey::CONFIG:
            ring::Manager::instance().getConfigurationManager()->registerEvHandlers(static_cast<ring::ring_config_ev_handlers*>(entry.second));
            break;
            case EventHandlerKey::PRESENCE:
            ring::Manager::instance().getPresenceManager()->registerEvHandlers(static_cast<ring::ring_pres_ev_handlers*>(entry.second));
            break;
            case EventHandlerKey::VIDEO:
            ring::Manager::instance().getVideoManager()->registerEvHandlers(static_cast<ring::ring_video_ev_handlers*>(entry.second));
            break;
        }
    }

    // Initialize manager now
    try {
        ring::Manager::instance().init("");
    } catch (...) {
        return -RING_ERR_MANAGER_INIT;
    }

    return 0;
}

void ring_fini(void)
{
    // Finish manager
    ring::Manager::instance().finish();
}

void ring_poll_events()
{
    ring::Manager::instance().pollEvents();
}
