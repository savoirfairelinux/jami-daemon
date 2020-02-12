#ifndef SYS_TIME_H_
#define SYS_TIME_H_

#include <time.h>
#include <winsock2.h>

struct timezone
{
    int  tz_minuteswest; /* minutes W of Greenwich */
    int  tz_dsttime;     /* type of dst correction */
};

static __inline int gettimeofday(struct timeval *tp, struct timezone * tzp)
{
    FILETIME    file_time;
    SYSTEMTIME  system_time;
    ULARGE_INTEGER ularge;
    static int tzflag;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    ularge.LowPart = file_time.dwLowDateTime;
    ularge.HighPart = file_time.dwHighDateTime;

    tp->tv_sec = (long)((ularge.QuadPart - 116444736000000000Ui64) / 10000000L);
    tp->tv_usec = (long)(system_time.wMilliseconds * 1000);

    if (NULL != tzp)
    {
        if (!tzflag)
        {
            _tzset();
            tzflag++;
        }
        tzp->tz_minuteswest = _timezone / 60;
        tzp->tz_dsttime = _daylight;
    }
    return 0;
}

#endif