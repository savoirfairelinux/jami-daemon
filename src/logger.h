/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#define LOGFILE "dring"

/**
 * Print something, coloring it depending on the level
 */
void logger(const int level, const char* format, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;
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

#define STR(EXP) #EXP
#define XSTR(X) STR(X)

// Do not remove the "| " in following without modifying vlogger() code
#define LOG_FORMAT(M, ...) FILE_NAME ":" XSTR(__LINE__) "| " M, ##__VA_ARGS__

#ifdef __ANDROID__

#include <android/log.h>

#ifndef APP_NAME
#define APP_NAME "libdring"
#endif /* APP_NAME */

#undef LOG_FORMAT
#define LOG_FORMAT(M, ...) "%s:%d | " M, FILE_NAME, __LINE__, ##__VA_ARGS__

// Avoid printing whole path on android
#define FILE_NAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// because everyone likes reimplementing the wheel
#define LOG_ERR     ANDROID_LOG_ERROR
#define LOG_WARNING ANDROID_LOG_WARN
#define LOG_INFO    ANDROID_LOG_INFO
#define LOG_DEBUG   ANDROID_LOG_DEBUG

#define LOGGER(M, LEVEL, ...) __android_log_print(LEVEL, APP_NAME, LOG_FORMAT(M, ##__VA_ARGS__))

#elif defined(_WIN32)

#include "winsyslog.h"

#define LOG_ERR     EVENTLOG_ERROR_TYPE
#define LOG_WARNING EVENTLOG_WARNING_TYPE
#define LOG_INFO    EVENTLOG_INFORMATION_TYPE
#define LOG_DEBUG   EVENTLOG_SUCCESS

#define FILE_NAME __FILE__

#define LOGGER(M, LEVEL, ...) logger(LEVEL, LOG_FORMAT(M, ##__VA_ARGS__))

#else

#include <syslog.h>

#define FILE_NAME __FILE__

#define LOGGER(M, LEVEL, ...) logger(LEVEL, LOG_FORMAT(M, ##__VA_ARGS__))

#endif /* __ANDROID__ _WIN32 */

#define RING_ERR(M, ...)   LOGGER(M, LOG_ERR, ##__VA_ARGS__)
#define RING_WARN(M, ...)  LOGGER(M, LOG_WARNING, ##__VA_ARGS__)
#define RING_INFO(M, ...)  LOGGER(M, LOG_INFO, ##__VA_ARGS__)
#define RING_DBG(M, ...)   LOGGER(M, LOG_DEBUG, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
