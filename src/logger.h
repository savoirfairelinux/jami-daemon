/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include "jami/def.h"

//#define __STDC_FORMAT_MACROS 1
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/chrono.h>

#include <cinttypes> // for PRIx64
#include <cstdarg>

#include <atomic>
#include <sstream>
#include <string>
#include "string_utils.h" // to_string

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

/**
 * Thread-safe function to print the stringified contents of errno
 */
void strErr();

///
/// Level-driven logging class that support printf and C++ stream logging fashions.
///
class Logger
{
public:

    class Handler;
    struct Msg;

    Logger(int level, const char* file, int line, bool linefeed)
        : level_ {level}
        , file_ {file}
        , line_ {line}
        , linefeed_ {linefeed}
    {}

    Logger() = delete;
    Logger(const Logger&) = delete;
    Logger(Logger&&) = default;

    ~Logger() { log(level_, file_, line_, linefeed_, "%s", os_.str().c_str()); }

    template<typename T>
    inline Logger& operator<<(const T& value)
    {
        os_ << value;
        return *this;
    }

    LIBJAMI_PUBLIC
    static void write(int level, const char* file, int line, std::string&& message);

    ///
    /// Printf fashion logging.
    ///
    /// Example: JAMI_DBG("%s", "Hello, World!")
    ///
    LIBJAMI_PUBLIC
    static void log(int level, const char* file, int line, bool linefeed, const char* const fmt, ...)
        PRINTF_ATTRIBUTE(5, 6);

    ///
    /// Printf fashion logging (using va_list parameters)
    ///
    LIBJAMI_PUBLIC
    static void vlog(int level, const char* file, int line, bool linefeed, const char* fmt, va_list);

    static void setConsoleLog(bool enable);
    static void setSysLog(bool enable);
    static void setMonitorLog(bool enable);
    static void setFileLog(const std::string& path);

    static void setDebugMode(bool enable);
    static bool debugEnabled();

    static void fini();

    ///
    /// Stream fashion logging.
    ///
    /// Example: JAMI_DBG() << "Hello, World!"
    ///
    static Logger log(int level, const char* file, int line, bool linefeed)
    {
        return {level, file, line, linefeed};
    }

private:

    int level_;              ///< LOG_XXXX values
    const char* const file_; ///< contextual filename (printed as header)
    const int line_;         ///< contextual line number (printed as header)
    bool linefeed_ {
        true}; ///< true if a '\n' (or any platform equivalent) has to be put at line end in consoleMode
    std::ostringstream os_; ///< string stream used with C++ stream style (stream operator<<)
};

namespace log {

template<typename S, typename... Args>
void info(const char* file, int line, S&& format, Args&&... args) {
    Logger::write(LOG_INFO, file, line, fmt::format(std::forward<S>(format), std::forward<Args>(args)...));
}

template<typename S, typename... Args>
void dbg(const char* file, int line, S&& format, Args&&... args) {
    Logger::write(LOG_DEBUG, file, line, fmt::format(std::forward<S>(format), std::forward<Args>(args)...));
}

template<typename S, typename... Args>
void warn(const char* file, int line, S&& format, Args&&... args) {
    Logger::write(LOG_WARNING, file, line, fmt::format(std::forward<S>(format), std::forward<Args>(args)...));
}

template<typename S, typename... Args>
void error(const char* file, int line, S&& format, Args&&... args) {
    Logger::write(LOG_ERR, file, line, fmt::format(std::forward<S>(format), std::forward<Args>(args)...));
}

}

// We need to use macros for contextual information
#define JAMI_INFO(...) ::jami::Logger::log(LOG_INFO, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define JAMI_DBG(...)  ::jami::Logger::log(LOG_DEBUG, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define JAMI_WARN(...) ::jami::Logger::log(LOG_WARNING, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define JAMI_ERR(...)  ::jami::Logger::log(LOG_ERR, __FILE__, __LINE__, true, ##__VA_ARGS__)

#define JAMI_XINFO(...) ::jami::Logger::log(LOG_INFO, __FILE__, __LINE__, false, ##__VA_ARGS__)
#define JAMI_XDBG(...)  ::jami::Logger::log(LOG_DEBUG, __FILE__, __LINE__, false, ##__VA_ARGS__)
#define JAMI_XWARN(...) ::jami::Logger::log(LOG_WARNING, __FILE__, __LINE__, false, ##__VA_ARGS__)
#define JAMI_XERR(...)  ::jami::Logger::log(LOG_ERR, __FILE__, __LINE__, false, ##__VA_ARGS__)

#define JAMI_LOG(formatstr, ...) ::jami::log::info(__FILE__, __LINE__, FMT_STRING(formatstr), ##__VA_ARGS__)
#define JAMI_DEBUG(formatstr, ...) if(::jami::Logger::debugEnabled()) { ::jami::log::dbg(__FILE__, __LINE__, FMT_STRING(formatstr), ##__VA_ARGS__); }
#define JAMI_WARNING(formatstr, ...) ::jami::log::warn(__FILE__, __LINE__, FMT_STRING(formatstr), ##__VA_ARGS__)
#define JAMI_ERROR(formatstr, ...) ::jami::log::error(__FILE__, __LINE__, FMT_STRING(formatstr), ##__VA_ARGS__)

} // namespace jami
