/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *
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
 */
#include <string>
#include <vector>
#include <map>
#include <cstdlib>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "manager.h"
#include "logger.h"
#include "dring.h"
#include "callmanager_interface.h"
#include "configurationmanager_interface.h"
#include "presencemanager_interface.h"
#include "client/ring_signal.h"

#ifdef ENABLE_VIDEO
#include "client/videomanager.h"
#endif // ENABLE_VIDEO

namespace DRing {

bool
init(enum InitFlag flags) noexcept
{
    ::setDebugMode(flags & DRING_FLAG_DEBUG);
    ::setConsoleLog(flags & DRING_FLAG_CONSOLE_LOG);

    // Following function create a local static variable inside
    // This var must have the same live as Manager.
    // So we call it now to create this var.
    jami::getSignalHandlers();

    try {
        // current implementation use static variable
        auto& manager = jami::Manager::instance();
        manager.setAutoAnswer(flags & DRING_FLAG_AUTOANSWER);
        return true;
    } catch (...) {
        return false;
    }
}

bool
start(const std::string& config_file) noexcept
{
    try {
        jami::Manager::instance().init(config_file);
    } catch (...) {
        return false;
    }
    return true;
}

void
fini() noexcept
{
    jami::Manager::instance().finish();
}

} // namespace DRing
