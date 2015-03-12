/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "logger.h"

static int consoleLog;
static int debugMode;

#ifdef WIN32
#include "winsyslog.h"
#endif

void logger(const int level, const char* format, ...)
{
    if (!debugMode && level == LOG_DEBUG)
        return;

    va_list ap;
    va_start(ap, format);
    vlogger(level, format, ap);
    va_end(ap);
}

void vlogger(const int level, const char *format, va_list ap)
{
#ifdef WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD saved_attributes;
#endif

    if (!debugMode && level == LOG_DEBUG)
        return;

    if (consoleLog) {
#ifndef _WIN32
        const char *color_prefix = "";
#else
        WORD color_prefix = FOREGROUND_GREEN;
#endif

        switch (level) {
            case LOG_ERR:
                color_prefix = RED;
                break;
            case LOG_WARNING:
                color_prefix = YELLOW;
                break;
        }

#ifndef _WIN32
        fputs(color_prefix, stderr);
#else
        GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
        saved_attributes = consoleInfo.wAttributes;
        SetConsoleTextAttribute(hConsole, color_prefix);
#endif

        vfprintf(stderr, format, ap);

#ifndef _WIN32
        fputs(END_COLOR"\n", stderr);
#else
        fputs("\n", stderr);
        SetConsoleTextAttribute(hConsole, saved_attributes);
#endif

    } else {
        vsyslog(level, format, ap);
    }
}

void setConsoleLog(int c)
{
    consoleLog = c;
}

void setDebugMode(int d)
{
    debugMode = d;
}

int getDebugMode(void)
{
    return debugMode;
}

void strErr(void)
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
