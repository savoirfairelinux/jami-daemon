AC_DEFUN([LP_SETUP_EXOSIP],[
AC_REQUIRE([AC_CANONICAL_HOST])
AC_REQUIRE([LP_CHECK_OSIP2])

dnl support for linux-thread or posix thread (pthread.h)
AC_ARG_ENABLE(pthread,
[  --enable-pthread        enable support for POSIX threads. (autodetect)],
enable_pthread=$enableval,enable_pthread="no")

dnl compile with mt support
if test "x$enable_pthread" = "xyes"; then
  EXOSIP_CFLAGS="-DHAVE_PTHREAD"
  EXOSIP_LIBS="-lpthread"
else
  ACX_PTHREAD()
fi

dnl eXosip embeded stuff
EXOSIP_CFLAGS="$OSIP_CFLAGS -DOSIP_MT -DENABLE_TRACE -DNEW_TIMER -DSM -DMSN_SUPPORT -DUSE_TMP_BUFFER"
EXOSIP_LIBS="$OSIP_LIBS"
AC_CHECK_HEADERS(semaphore.h)
AC_CHECK_HEADERS(sys/sem.h)
case $target in
  linux*)
     EXOSIP_CFLAGS="$EXOSIP_CFLAGS -pedantic"
     ;;
  irix*)
     ;;
  hpux* | hp-ux*)
     ;;
  aix*)
     ;;
  osf*)
     AC_CHECK_LIB(rt,sem_open,[EXOSIP_LIBS="$EXOSIP_LIBS -lrt"])
     ;;
  sunos*)
     ;;
  darwin*)
     EXOSIP_CFLAGS="$EXOSIP_CFLAGS -pedantic"
     ;;
  *)
     ;;
esac

AC_CHECK_LIB(posix4,sem_open,[EXOSIP_LIBS="$EXOSIP_LIBS -lposix4 -mt"])
AC_CHECK_LIB(nsl,nis_add,[EXOSIP_LIBS="$EXOSIP_LIBS -lnsl"])
AC_CHECK_LIB(socket,sendto,[EXOSIP_LIBS="$EXOSIP_LIBS -lsocket"])
AC_CHECK_LIB(rt,clock_gettime,[EXOSIP_LIBS="$EXOSIP_LIBS -lrt"])
dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(ctype.h)
AC_CHECK_HEADERS(string.h)
AC_CHECK_HEADERS(strings.h)
AC_CHECK_HEADERS(stdio.h)
AC_CHECK_HEADERS(stdlib.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(stdarg.h)
AC_CHECK_HEADERS(varargs.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(assert.h)
AC_CHECK_HEADERS(signal.h)
AC_CHECK_HEADERS(sys/signal.h)
AC_CHECK_HEADERS(malloc.h)
AC_CHECK_HEADERS(sys/select.h)
AC_CHECK_HEADERS(sys/types.h)
AC_CHECK_HEADERS(fcntl.h)

AC_SUBST(EXOSIP_CFLAGS)
AC_SUBST(EXOSIP_LIBS)
])
