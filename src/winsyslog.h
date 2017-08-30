/*
 * This header borrowed from Cygnus GNUwin32 project
 *
 * Cygwin is free software.  Red Hat, Inc. licenses Cygwin to you under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License either version 3 of the license, or (at your
 * option) any later version (GPLv3+), along with the additional permissions
 * given below.
 *
 * Modified for use with functions to map syslog
 * calls to EventLog calls on the windows platform
 *
 * much of this is not used, but here for the sake of
 * error free compilation.  EventLogs will most likely
 * not behave as syslog does, but may be useful anyway.
 * much of what syslog does can be emulated here, but
 * that will have to be done later.
 */

#ifndef WINSYSLOG_H
#define WINSYSLOG_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>

#define WINLOG_PRIMASK  0x07

#define WINLOG_PRI(p)   ((p) & WINLOG_PRIMASK)
#define WINLOG_MAKEPRI(fac, pri)    (((fac) << 3) | (pri))

#define WINLOG_KERN (0<<3)
#define WINLOG_USER (1<<3)
#define WINLOG_MAIL (2<<3)
#define WINLOG_DAEMON   (3<<3)
#define WINLOG_AUTH (4<<3)
#define WINLOG_SYSLOG   (5<<3)
#define WINLOG_LPR      (6<<3)
#define WINLOG_NEWS (7<<3)
#define WINLOG_UUCP (8<<3)
#define WINLOG_CRON (9<<3)
#define WINLOG_AUTHPRIV (10<<3)

#define WINLOG_NFACILITIES  10
#define WINLOG_FACMASK  0x03f8
#define WINLOG_FAC(p)   (((p) & WINLOG_FACMASK) >> 3)

#define WINLOG_MASK(pri)    (1 << (pri))
#define WINLOG_UPTO(pri)    ((1 << ((pri)+1)) - 1)

/*
 * Option flags for openlog.
 *
 * WINLOG_ODELAY no longer does anything.
 * WINLOG_NDELAY is the inverse of what it used to be.
 */
#define WINLOG_PID         0x01 /* log the pid with each message */
#define WINLOG_CONS        0x02 /* log on the console if errors in sending */
#define WINLOG_ODELAY      0x04 /* delay open until first syslog() (default) */
#define WINLOG_NDELAY      0x08 /* don't delay open */
#define WINLOG_NOWAIT      0x10 /* don't wait for console forks: DEPRECATED */
#define WINLOG_PERROR      0x20 /* log to stderr as well */

#define strerror_r(errno,buf,len) strerror_s(buf,len,errno)

#ifdef __cplusplus
extern "C" {
#endif

extern void closelog(void);
extern void openlog(const char *, int, int);
extern void vsyslog(int, const char *, va_list);

#ifdef __cplusplus
}
#endif
#endif
