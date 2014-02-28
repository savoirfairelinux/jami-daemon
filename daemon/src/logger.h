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

#ifndef LOGGER_H_
#define LOGGER_H_

#include <pthread.h>
#ifdef __ANDROID__
#include <cstring>
#include <android/log.h>
#else
#include <syslog.h>
#endif

namespace Logger {
void log(const int, const char*, ...);

void setConsoleLog(bool);
void setDebugMode(bool);
bool getDebugMode();
void strErr();
};

#define LOG_FORMAT(M, ...) "%s:%d:0x%x: " M, FILE_NAME, __LINE__, (unsigned long) pthread_self() & 0xffff, ##__VA_ARGS__

#ifndef __ANDROID__

#define FILE_NAME __FILE__
#define ERROR(M, ...)   LOGGER(M, LOG_ERR, ##__VA_ARGS__)
#define WARN(M, ...)    LOGGER(M, LOG_WARNING, ##__VA_ARGS__)
#define INFO(M, ...)    LOGGER(M, LOG_INFO, ##__VA_ARGS__)
#define DEBUG(M, ...)   LOGGER(M, LOG_DEBUG, ##__VA_ARGS__)
#define LOGGER(M, LEVEL, ...) Logger::log(LEVEL, LOG_FORMAT(M, ##__VA_ARGS__))

#else /* ANDROID */

#ifndef APP_NAME
#define APP_NAME "libsflphone"
#endif

// Avoid printing whole path on android
#define FILE_NAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define ERROR(M, ...)   LOGGER(M, ANDROID_LOG_ERROR, ##__VA_ARGS__)
#define WARN(M, ...)    LOGGER(M, ANDROID_LOG_WARN, ##__VA_ARGS__)
#define INFO(M, ...)    LOGGER(M, ANDROID_LOG_INFO, ##__VA_ARGS__)
#define DEBUG(M, ...)   LOGGER(M, ANDROID_LOG_DEBUG, ##__VA_ARGS__)
#define LOGGER(M, LEVEL, ...) __android_log_print(LEVEL, APP_NAME, LOG_FORMAT(M, ##__VA_ARGS__))
#endif /* ANDROID */

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


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
// No format strings/varargs allowed
#define THROW_ERROR(EXCEPTION_CLASS, M) throw EXCEPTION_CLASS(__FILE__ ":" TOSTRING(__LINE__) ":" M)

#endif // LOGGER_H_

