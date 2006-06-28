AC_DEFUN([LP_CHECK_EXOSIP2],[
AC_REQUIRE([LP_CHECK_OSIP2])

AC_ARG_WITH( exosip,
      [  --with-exosip    Set prefix where libexosip can be found (ex:/usr or /usr/local)@<:@default=/usr@:>@ ],
      [ exosip_prefix=${withval}],[ exosip_prefix=/usr ])
AC_SUBST(exosip_prefix)


EXOSIP_CFLAGS="-I$exosip_prefix/include $OSIP_CFLAGS"
EXOSIP_LIBS="-L$exosip_prefix/lib $OSIP_LIBS"
 
dnl support for linux-thread or posix thread (pthread.h)
AC_ARG_ENABLE(pthread,
[  --enable-pthread        enable support for POSIX threads. (autodetect)],
enable_pthread=$enableval,enable_pthread="no")

dnl compile with mt support
if test "x$enable_pthread" = "xyes"; then
  EXOSIP_CFLAGS="$EXOSIP_CFLAGS -DHAVE_PTHREAD"
  EXOSIP_LIBS="$EXOSIP_LIBS -lpthread"
else
  ACX_PTHREAD()
fi

dnl check exosip2 headers
CPPFLAGS_save=$CPPFLAGS
CPPFLAGS=$EXOSIP_CFLAGS
AC_CHECK_HEADER([eXosip2/eXosip.h], ,AC_MSG_ERROR([Could not find libexosip2 headers !]))
CPPFLAGS=$CPPFLAGS_save

dnl check for exosip2 libs
LDFLAGS_save=$LDFLAGS
LDFLAGS=$EXOSIP_LIBS
LIBS_save=$LIBS
AC_CHECK_LIB(eXosip2,eXosip_init, , AC_MSG_ERROR([Could not find osip2 libraries !]))
LDFLAGS=$LDFLAGS_save
LIBS=$LIBS_save

EXOSIP_LIBS="$EXOSIP_LIBS -leXosip2"

AC_SUBST(EXOSIP_CFLAGS)
AC_SUBST(EXOSIP_LIBS)

])

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
