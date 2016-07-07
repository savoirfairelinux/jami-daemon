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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#define LOGFILE "dring"

/**
 * Print something, coloring it depending on the level
 */
void logger(const int level, const char* format, ...)
#ifdef _WIN32
    __attribute__((format(gnu_printf, 2, 3)))
#elif defined(__GNUC__)
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

// Line return char in a string
#define ENDL "\n"

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

#define RING_ERR(M, ...)   LOGGER(M ENDL, LOG_ERR, ##__VA_ARGS__)
#define RING_WARN(M, ...)  LOGGER(M ENDL, LOG_WARNING, ##__VA_ARGS__)
#define RING_INFO(M, ...)  LOGGER(M ENDL, LOG_INFO, ##__VA_ARGS__)
#define RING_DBG(M, ...)   LOGGER(M ENDL, LOG_DEBUG, ##__VA_ARGS__)

#define RING_XERR(M, ...)   LOGGER(M, LOG_ERR, ##__VA_ARGS__)
#define RING_XWARN(M, ...)  LOGGER(M, LOG_WARNING, ##__VA_ARGS__)
#define RING_XINFO(M, ...)  LOGGER(M, LOG_INFO, ##__VA_ARGS__)
#define RING_XDBG(M, ...)   LOGGER(M, LOG_DEBUG, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
