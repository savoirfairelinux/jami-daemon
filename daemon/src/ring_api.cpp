/*
 *  Copyright (C) 2014-2015 Savoir-Faire Linux Inc.
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
#include "callmanager_interface.h"
#include "configurationmanager_interface.h"
#include "presencemanager_interface.h"

#ifdef RING_VIDEO
#include "client/videomanager.h"
#endif // RING_VIDEO

namespace DRing {

struct Daemon : ring::ManagerImpl {};

const char*
version() noexcept
{
    return PACKAGE_VERSION;
}

Daemon*
create_daemon(enum InitFlag flags) noexcept
{
    ::setDebugMode(flags & DRING_FLAG_DEBUG);
    ::setConsoleLog(flags & DRING_FLAG_CONSOLE_LOG);

    try {
        return static_cast<Daemon*>(&ring::Manager::instance());
    } catch (...) {
        return nullptr;
    }
}

void
set_daemon_event_handlers(Daemon* /*daemon*/, const std::map<EventHandlerKey, std::map<std::string, std::shared_ptr<CallbackWrapperBase>>>& ev_handlers) noexcept
{
    for (const auto& entry : ev_handlers) {
        switch(entry.first) {
            case EventHandlerKey::CALL:
                DRing::registerCallHandlers(entry.second);
            break;

            case EventHandlerKey::CONFIG:
                DRing::registerConfHandlers(entry.second);
            break;

            case EventHandlerKey::PRESENCE:
                DRing::registerPresHandlers(entry.second);
            break;

            case EventHandlerKey::VIDEO:
#ifdef RING_VIDEO
                DRing::registerVideoHandlers(entry.second);
#else
            RING_ERR("Error: linking video handler without video support");
#endif
            break;
        }
    }
}

bool
start_daemon(Daemon* daemon, const std::string& config_file) noexcept
{
    try {
        daemon->init(config_file);
    } catch (...) {
        return false;
    }
    return true;
}

void
delete_daemon(Daemon* daemon) noexcept
{
    daemon->finish();
}

void
poll_daemon_events(Daemon* daemon) noexcept
{
    daemon->pollEvents();
}

} // namespace DRing
