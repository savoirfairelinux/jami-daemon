/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion>@savoirfairelinux.com>
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

#include <syslog.h>
#include <stdio.h>
#include <cstdlib>

#include "lib/supervisor.h"
#include "lib/syslog.h"

/* Jami */
#include "logger.h"

__weak
bool
syslog_handler(int priority, const char *fmt, va_list ap)
{
        (void)priority;
        (void)fmt;
        (void)ap;

        fflush(NULL);
        fprintf(stderr, "libfuzz: stop by supervisor log\n");

        return false;
}

BEGIN_WRAPPER(void, vsyslog, int priority, const char* format, va_list ap)
{
    static int priority_threshold = -2;

    if (-2 == priority_threshold) {

        const char* str_to_int[] = {
                [LOG_EMERG]   = "EMERG",
                [LOG_ALERT]   = "ALERT",
                [LOG_CRIT]    = "CRIT",
                [LOG_ERR]     = "ERR",
                [LOG_WARNING] = "WARNING",
                [LOG_NOTICE]  = "NOTICE",
                [LOG_INFO]    = "INFO",
                [LOG_DEBUG]   = "DEBUG"
        };

        const char* supervisor_says = std::getenv(supervisor::env::log);

        if (nullptr == supervisor_says) {
            priority_threshold = 0;
            goto no_threshold;
        }

        for (size_t i = 0; i < array_size(str_to_int); ++i) {
            if (streq(str_to_int[i], supervisor_says)) {
                priority_threshold = i;
                break;
            }
        }

        if (priority_threshold < 0) {
            fprintf(stderr, "libfuzz: Invalid value of SUPERVISOR_LOG `%s`\n", supervisor_says);
            priority_threshold = -1;
        }
    }

no_threshold:
    this_func(priority, format, ap);

    if (priority <= priority_threshold) {
       if (not syslog_handler(priority, format, ap)) {
            exit(supervisor::signal::exit::log);
        }
    }
}
END_WRAPPER();
