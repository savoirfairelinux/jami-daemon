AC_INIT([SFLphone],[0.9.5~beta],[sflphoneteam@savoirfairelinux.com],[sflphone])
AM_CONFIG_HEADER(config.h)

LT_INIT

PACKAGE=SFLphone
#VERSION=0.9.5~beta
VERSION=`head -n 1 debian/changelog | awk '{print $2}' | sed -e 's/(//g' | sed -e 's/)//g'`

AM_INIT_AUTOMAKE($PACKAGE,$VERSION)
AC_CONFIG_MACRO_DIR([m4])

AC_ARG_WITH(debug,
		AS_HELP_STRING(
			[--with-debug],
			[Set 'full' to enable debugging information @<:@default=no@:>@]
		),
		[with_debug=${withval}],
		[with_debug=no]
	)
if test "x$with_debug" = "xfull" -o "x$with_debug" = "xyes"; then
	CFLAGS="$CFLAGS -g -O2 -DDEBUG -Wall -Wextra"
else
	CFLAGS="$CFLAGS -g -Wall -O2"
fi

AC_PROG_CC
AC_PROG_INSTALL
AC_STDC_HEADERS
AC_PROG_LIBTOOL

dnl Check for log4c
AC_CHECK_HEADERS(log4c.h, have_log4c=true, have_log4c=false)
if ! $have_log4c; then
    AC_MSG_ERROR(Please install the log4c library)
fi

dnl Check for GTK+ version
dnl If gtk+-2.0 >= 2.16, we don't need libsexy package anymore
PKG_CHECK_MODULES(GTK216, gtk+-2.0 >= 2.16, need_libsexy=false, need_libsexy=true)
if  $need_libsexy; then
    PKG_CHECK_MODULES(LIBSEXY, libsexy >= 0.1, have_libsexy=true, have_libsexy=false)
    if ! $have_libsexy; then
        AC_MSG_ERROR(libsexy package has not been found. Please install it.)
    fi
fi
AC_SUBST(LIBSEXY_CFLAGS)
AC_SUBST(LIBSEXY_LIBS)

PKG_CHECK_MODULES(DEPS,     \
    dbus-glib-1 >= 0.35     \
    libnotify >= 0.4        \
    gtk+-2.0 >= 2.2         \
    glib-2.0 >= 2.2         \
    libebook-1.2 >= 2.22
)

AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

# This macro is defined in check.m4 and tests if check.h and
# libcheck.a are installed in your system. It sets CHECK_CFLAGS and
# CHECK_LIBS accordingly.  
#  AM_PATH_CHECK([MINIMUM-VERSION,
#                [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl AM_PATH_CHECK() DEPRECATED
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

AC_OUTPUT([
Makefile 
src/Makefile
src/config/Makefile
src/dbus/Makefile
src/contacts/Makefile
src/contacts/addressbook/Makefile
pixmaps/Makefile
sflphone.desktop
tests/Makefile
man/Makefile
])


