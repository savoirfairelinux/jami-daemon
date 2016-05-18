/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include <string>
#include <sstream>
#include <iomanip>
#include <ios>
#include <mutex>
#include <thread>

#include "logger.h"

#ifdef __linux__
#include <syslog.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif // __linux__

#ifdef WIN32
#include "winsyslog.h"
#endif

#define BLACK "\033[22;30m"
#define GREEN "\033[22;32m"
#define BROWN "\033[22;33m"
#define BLUE "\033[22;34m"
#define MAGENTA "\033[22;35m"
#define GREY "\033[22;37m"
#define DARK_GREY "\033[01;30m"
#define LIGHT_RED "\033[01;31m"
#define LIGHT_SCREEN "\033[01;32m"
#define LIGHT_BLUE "\033[01;34m"
#define LIGHT_MAGENTA "\033[01;35m"
#define LIGHT_CYAN "\033[01;36m"
#define WHITE "\033[01;37m"
#define END_COLOR "\033[0m"

#ifndef _WIN32
#define RED "\033[22;31m"
#define YELLOW "\033[01;33m"
#define CYAN "\033[22;36m"
#else
#define RED FOREGROUND_RED
#define YELLOW FOREGROUND_RED + FOREGROUND_GREEN
#define CYAN FOREGROUND_BLUE + FOREGROUND_GREEN
#endif

static int consoleLog;
static int debugMode;
static std::mutex logMutex;

static std::string
getHeader(const char* ctx)
{
#ifdef __linux__
    auto tid = syscall(__NR_gettid) & 0xffff;
#else
    auto tid = std::this_thread::get_id();
#endif // __linux__

    // Timestamp
    unsigned int secs, milli;
    struct timeval tv;
    if (!gettimeofday(&tv, NULL)) {
        secs = tv.tv_sec;
        milli = tv.tv_usec / 1000; // suppose that milli < 1000
    } else {
        secs = time(NULL);
        milli = 0;
    }

    std::ostringstream out;
    const auto prev_fill = out.fill();
    out << '[' << secs
        << '.' << std::right << std::setw(3) << std::setfill('0') << milli << std::left
        << '|' << std::right << std::setw(5) << std::setfill(' ') << tid << std::left;
    out.fill(prev_fill);

    // Context
    if (ctx)
        out << "|" << std::setw(24) << ctx;

    out << "] ";

    return out.str();
}

void
logger(const int level, const char* format, ...)
{
    if (!debugMode && level == LOG_DEBUG)
        return;

    va_list ap;
    va_start(ap, format);
    vlogger(level, format, ap);
    va_end(ap);
}

void
vlogger(const int level, const char *format, va_list ap)
{
    if (!debugMode && level == LOG_DEBUG)
        return;

    // syslog is supposed to thread-safe, but not all implementations (Android?)
    // follow strictly POSIX rules... so we lock our mutex in any cases.
    std::lock_guard<std::mutex> lk {logMutex};

    if (consoleLog) {
#ifndef _WIN32
        const char* color_header = CYAN;
        const char* color_prefix = "";
#else
        WORD color_header = CYAN;
        WORD color_prefix = FOREGROUND_GREEN;
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
        WORD saved_attributes;
#endif

        switch (level) {
            case LOG_ERR:
                color_prefix = RED;
                break;
            case LOG_WARNING:
                color_prefix = YELLOW;
                break;
        }

#ifdef _WIN32
        GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
        saved_attributes = consoleInfo.wAttributes;
#endif

        // must exist, check LOG_FORMAT
        auto sep = strchr(format, '|');
        if (sep) {
#ifndef _WIN32
            fputs(color_header, stderr);
#else
            SetConsoleTextAttribute(hConsole, color_header);
#endif
            std::string ctx(format, sep - format);
            format = sep + 2;
            fputs(getHeader(ctx.c_str()).c_str(), stderr);
#ifndef _WIN32
            fputs(END_COLOR, stderr);
#else
            SetConsoleTextAttribute(hConsole, saved_attributes);
#endif
        }
#ifndef _WIN32
        fputs(color_prefix, stderr);
#else
        SetConsoleTextAttribute(hConsole, color_prefix);
#endif

        vfprintf(stderr, format, ap);
        if (not sep)
            fputs(ENDL, stderr);

#ifndef _WIN32
        fputs(END_COLOR, stderr);
#else
        SetConsoleTextAttribute(hConsole, saved_attributes);
#endif

    } else {
        vsyslog(level, format, ap);
    }
}

void
setConsoleLog(int c)
{
    if (c)
        closelog();
    else {
#ifdef _WIN32
        openlog(LOGFILE, WINLOG_PID, WINLOG_MAIL);
#else
        openlog(LOGFILE, LOG_NDELAY, LOG_USER);
#endif /* _WIN32 */
    }

    consoleLog = c;
}

void
setDebugMode(int d)
{
    debugMode = d;
}

int
getDebugMode(void)
{
    return debugMode;
}

void
strErr(void)
{
#ifdef __GLIBC__
    RING_ERR("%m");
#else
    char buf[1000];
    const char *errstr;

    switch (strerror_r(errno, buf, sizeof(buf))) {
        case 0:
            errstr = buf;
            break;
        case ERANGE: /* should never happen */
            errstr = "unknown (too big to display)";
            break;
        default:
            errstr = "unknown (invalid error number)";
            break;
    }

    RING_ERR("%s", errstr);
#endif
}
