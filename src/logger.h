/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#pragma once

//#define __STDC_FORMAT_MACROS 1
#include <cinttypes> // for PRIx64
#include <cstdarg>

#include <sstream>
#include <string>
#include "string_utils.h" // to_string

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allow writing on the console
 */
void setConsoleLog(int c);

/**
 * When debug mode is not set, logging will not print anything
 */
void setDebugMode(int d);

/**
 * Return the current mode
 */
int getDebugMode(void);

/**
 * Thread-safe function to print the stringified contents of errno
 */
void strErr();

#ifdef __cplusplus
}
#endif

#ifdef __ANDROID__

#include <android/log.h>
#define LOG_ERR     ANDROID_LOG_ERROR
#define LOG_WARNING ANDROID_LOG_WARN
#define LOG_INFO    ANDROID_LOG_INFO
#define LOG_DEBUG   ANDROID_LOG_DEBUG

#elif defined(_WIN32)

#include "winsyslog.h"
#define LOG_ERR     EVENTLOG_ERROR_TYPE
#define LOG_WARNING EVENTLOG_WARNING_TYPE
#define LOG_INFO    EVENTLOG_INFORMATION_TYPE
#define LOG_DEBUG   EVENTLOG_SUCCESS

#else

#include <syslog.h> // Defines LOG_XXXX

#endif /* __ANDROID__ / _WIN32 */

#if defined(_WIN32) && !defined(_MSC_VER)
#define PRINTF_ATTRIBUTE(a, b) __attribute__((format(gnu_printf, a, b)))
#elif defined(__GNUC__)
#define PRINTF_ATTRIBUTE(a, b) __attribute__((format(printf, a, b)))
#else
#define PRINTF_ATTRIBUTE(a, b)
#endif

namespace jami {

///
/// Level-driven logging class that support printf and C++ stream logging fashions.
///
class Logger
{
public:
    Logger(int level, const char* file, int line, bool linefeed)
        : level_ {level}
        , file_ {file}
        , line_ {line}
        , linefeed_ {linefeed} {}

    Logger() = delete;
    Logger(const Logger&) = default;
    Logger(Logger&&) = default;

    ~Logger() {
        log(level_, file_, line_, linefeed_, "%s", os_.str().c_str());
    }

    template <typename T>
    inline Logger& operator<<(const T& value) {
        os_ << value;
        return *this;
    }

    ///
    /// Printf fashion logging.
    ///
    /// Example: JAMI_DBG("%s", "Hello, World!")
    ///
    static void log(int level, const char* file, int line, bool linefeed,
                    const char* const fmt, ...) PRINTF_ATTRIBUTE(5, 6);

    ///
    /// Printf fashion logging (using va_list parameters)
    ///
    static void vlog(const int level, const char* file, int line, bool linefeed,
                     const char* format, va_list);

    ///
    /// Stream fashion logging.
    ///
    /// Example: JAMI_DBG() << "Hello, World!"
    ///
    static Logger log(int level, const char* file, int line, bool linefeed) {
        return {level, file, line, linefeed};
    }

private:
    int level_;                 ///< LOG_XXXX values
    const char* const file_;    ///< contextual filename (printed as header)
    const int line_;            ///< contextual line number (printed as header)
    bool linefeed_ {true};      ///< true if a '\n' (or any platform equivalent) has to be put at line end in consoleMode
    std::ostringstream os_;     ///< string stream used with C++ stream style (stream operator<<)
};

// We need to use macros for contextual information
#define JAMI_INFO(...) ::jami::Logger::log(LOG_INFO, __FILE__, __LINE__, true, ## __VA_ARGS__)
#define JAMI_DBG(...) ::jami::Logger::log(LOG_DEBUG, __FILE__, __LINE__, true, ## __VA_ARGS__)
#define JAMI_WARN(...) ::jami::Logger::log(LOG_WARNING, __FILE__, __LINE__, true, ## __VA_ARGS__)
#define JAMI_ERR(...) ::jami::Logger::log(LOG_ERR, __FILE__, __LINE__, true, ## __VA_ARGS__)

#define JAMI_XINFO(...) ::jami::Logger::log(LOG_INFO, __FILE__, __LINE__, false, ## __VA_ARGS__)
#define JAMI_XDBG(...) ::jami::Logger::log(LOG_DEBUG, __FILE__, __LINE__, false, ## __VA_ARGS__)
#define JAMI_XWARN(...) ::jami::Logger::log(LOG_WARNING, __FILE__, __LINE__, false, ## __VA_ARGS__)
#define JAMI_XERR(...) ::jami::Logger::log(LOG_ERR, __FILE__, __LINE__, false, ## __VA_ARGS__)

} // namespace jami
