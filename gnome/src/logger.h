/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

void internal_log (const int level, const char* format, ...);
void set_log_level (const int level);

#define LOG_ERR 1
#define LOG_WARN 2
#define LOG_INFO 3
#define LOG_DEBUG 4

#define INTERNAL_LOG(M, LEVEL, ...) internal_log(LEVEL, "%s:%d: " M, __FILE__, \
                                                 __LINE__, ##__VA_ARGS__)

#define ERROR(M, ...)     INTERNAL_LOG(M, LOG_ERR, ##__VA_ARGS__)
#define WARN(M, ...)      INTERNAL_LOG(M, LOG_WARN, ##__VA_ARGS__)
#define INFO(M, ...)      INTERNAL_LOG(M, LOG_INFO, ##__VA_ARGS__)
#define DEBUG(M, ...)     INTERNAL_LOG(M, LOG_DEBUG, ##__VA_ARGS__)

/* Prints an error message and returns if the pointer A is NULL */
#define RETURN_IF_NULL(A, M, ...) \
    if (!(A)) { ERROR(M, ##__VA_ARGS__); return; }

#endif // LOGGER_H_
