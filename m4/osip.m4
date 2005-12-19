AC_DEFUN([LP_CHECK_OSIP2],[

AC_ARG_WITH( osip,
      [  --with-osip      Set prefix where osip can be found (ex:/usr or /usr/local)[default=/usr/local] ],
      [ osip_prefix=${withval}],[ osip_prefix=/usr ])
AC_SUBST(osip_prefix)


OSIP_CFLAGS="-I$osip_prefix/include"
OSIP_LIBS="-L$osip_prefix/lib"

dnl check osip2 headers
CPPFLAGS_save=$CPPFLAGS
CPPFLAGS=$OSIP_CFLAGS
AC_CHECK_HEADER([osip2/osip.h], ,AC_MSG_ERROR([Could not find osip2 headers !]))
CPPFLAGS=$CPPFLAGS_save

dnl check for osip2 libs
LDFLAGS_save=$LDFLAGS
LDFLAGS=$OSIP_LIBS
dnl AC_CHECK_LIB adds osipparser2 to LIBS, I don't want that !
LIBS_save=$LIBS
AC_CHECK_LIB(osipparser2,osip_message_init, , AC_MSG_ERROR([Could not find osip2 libraries !]))
LDFLAGS=$LDFLAGS_save
LIBS=$LIBS_save

OSIP_LIBS="$OSIP_LIBS -losipparser2 -losip2"

AC_SUBST(OSIP_CFLAGS)
AC_SUBST(OSIP_LIBS)

])
