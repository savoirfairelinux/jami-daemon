/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include <asio.hpp>
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

InitFlag initFlags = {};

bool
init(enum InitFlag flags) noexcept
{
    initFlags = flags;
    jami::Logger::setDebugMode(LIBJAMI_FLAG_DEBUG == (flags & LIBJAMI_FLAG_DEBUG));
    jami::Logger::setSysLog(LIBJAMI_FLAG_SYSLOG == (flags & LIBJAMI_FLAG_SYSLOG));
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
        if (flags & LIBJAMI_FLAG_NO_AUTOSYNC)
            manager.syncOnRegister = false;
    } catch (...) {
        return false;
    }

#ifdef __linux__
    // HACK: ignore system-wide GnuTLS configuration
    //
    // Since version 3.6.9, GnuTLS makes it possible to selectively disable algorithms
    // and protocols via a global configuration file. In particular, RSA PKCS1 v1.5
    // encryption can be disabled by setting the "allow-rsa-pkcs1-encrypt" option to
    // false. Doing this makes OpenDHT's putEncrypted function fail systematically and
    // therefore breaks several major features in Jami (e.g. sending contact requests).
    // As of December 2025, this is an issue on AlmaLinux 10 (there are other distributions
    // supported by Jami, including Ubuntu and Fedora, that include a system-wide
    // configuration file for GnuTLS, but for now they don't disable RSA PKCS1 v1.5).
    //
    // Some of the options in the configuration file can be bypassed by calling the right
    // function, but GnuTLS currently does not allow this in the case of RSA PKCS1 v1.5.
    // As a workaround, we take advantage of the fact that the location of the configuration
    // file can be changed at runtime via the GNUTLS_SYSTEM_PRIORITY_FILE environment
    // variable.
    static bool gnutlsInitialized = false;
    if (!gnutlsInitialized) {
        setenv("GNUTLS_SYSTEM_PRIORITY_FILE", "/dev/null", 1);
        // GnuTLS has already been initialized (in a library constructor) by the time we set
        // GNUTLS_SYSTEM_PRIORITY_FILE, so we need to reinitialize it in order for the new
        // value to be taken into account.
        gnutls_global_deinit();
        if (gnutls_global_init() < 0) {
            JAMI_ERROR("Failed to intialize gnutls");
            return false;
        }
        gnutlsInitialized = true;
    }
#endif

    return true;
}

bool
start(const std::filesystem::path& config_file) noexcept
{
    try {
        jami::Manager::instance().init(config_file, initFlags);
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

void
CallbackWrapperBase::post(std::function<void()> cb)
{
    if (auto io = jami::Manager::instance().ioContext())
        asio::post(*io, std::move(cb));
}

} // namespace libjami
