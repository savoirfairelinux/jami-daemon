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

const char*
version() noexcept
{
    return PACKAGE_VERSION;
}

bool
init(enum InitFlag flags) noexcept
{
    ::setDebugMode(flags & DRING_FLAG_DEBUG);
    ::setConsoleLog(flags & DRING_FLAG_CONSOLE_LOG);

    try {
        // current implementation use static variable
        return &ring::Manager::instance() != nullptr;
    } catch (...) {
        return nullptr;
    }
}

bool
start(const std::string& config_file) noexcept
{
    try {
        ring::Manager::instance().init(config_file);
    } catch (...) {
        return false;
    }
    return true;
}

void
fini() noexcept
{
    ring::Manager::instance().finish();
}

void
pollEvents() noexcept
{
    ring::Manager::instance().pollEvents();
}

} // namespace DRing
