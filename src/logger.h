/*
 *  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
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

#include <sstream>
#include <string>
#include "string_utils.h" // to_string

extern "C" {

/**
 * Print something, coloring it depending on the level
 */

void vlogger(const int level, const char* format, va_list);

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

}

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

namespace ring {

///
/// Level-driven logging class that support printf and C++ stream logging fashions.
///
class Logger
{
public:
#ifdef RING_UWP
    static constexpr auto ENDL = "";
#else
    static constexpr auto ENDL = "\n";
#endif

    Logger(int level, const char* file, int line, const char* func, const char* endl = ENDL)
        : level_ {level}
        , head_ {std::string(lastPathComponent(file))+":"+to_string(line)+"| "}
        , endl_ {endl} {
            (void)func;
        }
    Logger() = delete;
    Logger(const Logger&) = default;
    Logger(Logger&&) = default;

    ~Logger() {
        if (done_)
            return;
        log("%s", os_.str().c_str());
    }

    template <typename T>
    inline Logger& operator<<(const T& value) {
        os_ << value;
        return *this;
    }

    ///
    /// Printf fashion logging.
    ///
    /// Example: RING_DBG("%s", "Hello, World!")
    ///
    void log(const char* const fmt, ...)
#if defined(_WIN32) && !defined(RING_UWP)
        __attribute__((format(gnu_printf, 2, 3)))
#elif defined(__GNUC__)
        __attribute__((format(printf, 2, 3)))
#endif
        ;

    ///
    /// Stream fashion logging.
    ///
    /// Example: RING_DBG() << "Hello, World!"
    ///
    Logger& log() { return *this; }

private:
    const char* lastPathComponent(const char* path);

    bool done_ {false}; ///< true to prevent dtor to print the stream (i.e. when printf-style log(fmt, ...) is called)
    int level_; ///< LOG_XXXX values
    const std::string head_; ///< pre-computed header sent to internal logger engine
    const std::string endl_; ///< value to add at end of logged line
    std::ostringstream os_; ///< string stream used with C++ stream style (stream operator<<)
};

// We need to use macros for contextual information
#define RING_INFO ::ring::Logger(LOG_INFO, __FILE__, __LINE__, __func__).log
#define RING_DBG ::ring::Logger(LOG_DEBUG, __FILE__, __LINE__, __func__).log
#define RING_WARN ::ring::Logger(LOG_WARNING, __FILE__, __LINE__, __func__).log
#define RING_ERR ::ring::Logger(LOG_ERR, __FILE__, __LINE__, __func__).log

#define RING_XINFO ::ring::Logger(LOG_INFO, __FILE__, __LINE__, __func__, "").log
#define RING_XDBG ::ring::Logger(LOG_DEBUG, __FILE__, __LINE__, __func__, "").log
#define RING_XWARN ::ring::Logger(LOG_WARNING, __FILE__, __LINE__, __func__, "").log
#define RING_XERR ::ring::Logger(LOG_ERR, __FILE__, __LINE__, __func__, "").log

} // namespace ring
