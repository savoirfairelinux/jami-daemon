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

#include "winsyslog.h"
#include <stdio.h>
#include <fcntl.h>
#include <process.h>
#include <stdlib.h>

#ifndef THREAD_SAFE
static char *loghdr;            /* log file header string */
static HANDLE loghdl = NULL;    /* handle of event source */
#endif

static CHAR *                       //   return error message
getLastErrorText(                   // converts "Lasr Error" code into text
    CHAR *pBuf,                     //   message buffer
    ULONG bufSize)                  //   buffer size
{
    DWORD retSize;
    LPTSTR pTemp=NULL;

    if (bufSize < 16) {
        if (bufSize > 0) {
           pBuf[0]='\0';
       }
       return(pBuf);
   }
   retSize=FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
       FORMAT_MESSAGE_FROM_SYSTEM|
       FORMAT_MESSAGE_ARGUMENT_ARRAY,
       NULL,
       GetLastError(),
       LANG_NEUTRAL,
       (LPTSTR)&pTemp,
       0,
       NULL);
   if (!retSize || pTemp == NULL) {
        pBuf[0]='\0';
    }
    else {
        pTemp[strlen(pTemp)-2]='\0'; //remove cr and newline character
        sprintf(pBuf,"%0.*s (0x%x)",bufSize-16,pTemp,GetLastError());
        LocalFree((HLOCAL)pTemp);
    }
    return(pBuf);
}

void closelog(void)
{
    DeregisterEventSource(loghdl);
    free(loghdr);
}

/* Emulator for GNU syslog() routine
 * Accepts: priority
 *      format
 *      arglist
 */
// TODO: use a real EventIdentifier with a Message Text file ?
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa363679%28v=vs.85%29.aspx
void syslog(int level, const char* format, ...)
 {
    CONST CHAR *arr[1];
    char tmp[1024];

    va_list arglist;

    va_start(arglist, format);

    vsnprintf(tmp, 1024, format, arglist);

    arr[0] = tmp;
    BOOL err = ReportEvent(loghdl, (unsigned short) level, (unsigned short)level,
        level, NULL, 1, 0, arr, NULL);

    if (err == 0)
    {
        CHAR errText[1024];
        puts(getLastErrorText(errText, 1024));
    }

    va_end(arglist);
}

/* Emulator for BSD openlog() routine
 * Accepts: identity
 *      options
 *      facility
 */
 void openlog(const char *ident, int logopt, int facility)
 {
    char tmp[1024];

    if (loghdl) {
        closelog();
    }
    loghdl = RegisterEventSource(NULL, ident);
    sprintf(tmp, (logopt & WINLOG_PID) ? "%s[%d]" : "%s", ident, getpid());
    loghdr = _strdup(tmp);  /* save header for later */
}
