#include "winsyslog.h"
#include <stdio.h>
#include <fcntl.h>
#include <process.h>
#include <stdlib.h>

#ifndef THREAD_SAFE
static char *loghdr;			/* log file header string */
static HANDLE loghdl = NULL;	/* handle of event source */
#endif

static CHAR *                      //   return error message
getLastErrorText(                  // converts "Lasr Error" code into text
CHAR *pBuf,                        //   message buffer
ULONG bufSize)                     //   buffer size
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
													 NULL );
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

/* Emulator for GNU vsyslog() routine
 * Accepts: priority
 *      format
 *      arglist
 */
// TODO: use a real EventIdentifier with a Message Text file ?
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa363679%28v=vs.85%29.aspx
void vsyslog(int level, const char* format, va_list arglist)
{
	CONST CHAR *arr[1];
		char tmp[1024];

		if (!loghdl)
				openlog(LOGFILE, WINLOG_PID, WINLOG_MAIL);

		vsprintf(tmp, format, arglist);

		arr[0] = tmp;
		BOOL err = ReportEvent(loghdl, (unsigned short) level, (unsigned short)level,
													level, NULL, 1, 0, arr, NULL);

		if (err == 0)
		{
			CHAR errText[1024];
			puts(getLastErrorText(errText, 1024));
		}
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
	loghdr = _strdup(tmp);	/* save header for later */
}