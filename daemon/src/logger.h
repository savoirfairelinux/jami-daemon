/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef H_LOGGER
#define H_LOGGER

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pthread.h>

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <syslog.h>
#endif

/**
 * Print something, coloring it depending on the level
 */
void logger(const int level, const char *format, ...);

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

#define LOG_FORMAT(M, ...) "%s:%d:0x%x: " M, FILE_NAME, __LINE__, \
                           (unsigned long) pthread_self() & 0xffff, \
                           ##__VA_ARGS__

#ifndef __ANDROID__

#define FILE_NAME __FILE__
#define ERROR(M, ...)   LOGGER(M, LOG_ERR, ##__VA_ARGS__)
#define WARN(M, ...)    LOGGER(M, LOG_WARNING, ##__VA_ARGS__)
#define INFO(M, ...)    LOGGER(M, LOG_INFO, ##__VA_ARGS__)
#define DEBUG(M, ...)   LOGGER(M, LOG_DEBUG, ##__VA_ARGS__)
#define LOGGER(M, LEVEL, ...) logger(LEVEL, LOG_FORMAT(M, ##__VA_ARGS__))

#else /* __ANDROID__ */

#ifndef APP_NAME
#define APP_NAME "libsflphone"
#endif /* APP_NAME */

// Avoid printing whole path on android
#define FILE_NAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 \
                                          : __FILE__)

#define ERROR(M, ...)   LOGGER(M, ANDROID_LOG_ERROR, ##__VA_ARGS__)
#define WARN(M, ...)    LOGGER(M, ANDROID_LOG_WARN, ##__VA_ARGS__)
#define INFO(M, ...)    LOGGER(M, ANDROID_LOG_INFO, ##__VA_ARGS__)
#define DEBUG(M, ...)   LOGGER(M, ANDROID_LOG_DEBUG, ##__VA_ARGS__)
#define LOGGER(M, LEVEL, ...) __android_log_print(LEVEL, APP_NAME, LOG_FORMAT(M, ##__VA_ARGS__))
#endif /* __ANDROID__ */

#define BLACK "\033[22;30m"
#define RED "\033[22;31m"
#define GREEN "\033[22;32m"
#define BROWN "\033[22;33m"
#define BLUE "\033[22;34m"
#define MAGENTA "\033[22;35m"
#define CYAN "\033[22;36m"
#define GREY "\033[22;37m"
#define DARK_GREY "\033[01;30m"
#define LIGHT_RED "\033[01;31m"
#define LIGHT_SCREEN "\033[01;32m"
#define YELLOW "\033[01;33m"
#define LIGHT_BLUE "\033[01;34m"
#define LIGHT_MAGENTA "\033[01;35m"
#define LIGHT_CYAN "\033[01;36m"
#define WHITE "\033[01;37m"
#define END_COLOR "\033[0m"

#ifdef __cplusplus
}
#endif

#endif // H_LOGGER
