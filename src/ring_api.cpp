/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include "jami.h"
#include "callmanager_interface.h"
#include "configurationmanager_interface.h"
#include "presencemanager_interface.h"
#include "client/ring_signal.h"

#ifdef ENABLE_VIDEO
#include "client/videomanager.h"
#endif // ENABLE_VIDEO

namespace libjami {

bool
init(enum InitFlag flags) noexcept
{
    jami::Logger::setDebugMode(LIBJAMI_FLAG_DEBUG == (flags & LIBJAMI_FLAG_DEBUG));

    jami::Logger::setSysLog(true);
    jami::Logger::setConsoleLog(LIBJAMI_FLAG_CONSOLE_LOG == (flags & LIBJAMI_FLAG_CONSOLE_LOG));

    const char* log_file = getenv("JAMI_LOG_FILE");

    if (log_file) {
        jami::Logger::setFileLog(log_file);
    }

    // Following function create a local static variable inside
    // This var must have the same live as Manager.
    // So we call it now to create this var.
    jami::getSignalHandlers();

    try {
        // current implementation use static variable
        auto& manager = jami::Manager::instance();
        manager.setAutoAnswer(flags & LIBJAMI_FLAG_AUTOANSWER);

#if TARGET_OS_IOS
        if (flags & LIBJAMI_FLAG_IOS_EXTENSION)
            manager.isIOSExtension = true;
#endif

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

bool
initialized() noexcept
{
    return jami::Manager::initialized;
}

void
fini() noexcept
{
    jami::Manager::instance().finish();
    jami::Logger::fini();
}

void
logging(const std::string& whom, const std::string& action) noexcept
{
    if ("syslog" == whom) {
        jami::Logger::setSysLog(not action.empty());
    } else if ("console" == whom) {
        jami::Logger::setConsoleLog(not action.empty());
    } else if ("monitor" == whom) {
        jami::Logger::setMonitorLog(not action.empty());
    } else if ("file" == whom) {
        jami::Logger::setFileLog(action);
    } else {
        JAMI_ERR("Bad log handler %s", whom.c_str());
    }
}

} // namespace libjami
