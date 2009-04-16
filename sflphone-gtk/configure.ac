AC_INIT([SFLphone],[0.9.4],[sflphoneteam@savoirfairelinux.com],[sflphone])
AM_CONFIG_HEADER(config.h)

LT_INIT

PACKAGE=SFLphone
VERSION=0.9.4

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

AC_CHECK_HEADERS(log4c.h, have_log4c=true, have_log4c=false)
if ! $have_log4c; then
    AC_MSG_ERROR(Please install the log4c library)
fi

PKG_CHECK_MODULES(DEPS, \
    dbus-glib-1 >= 0.35 \
    libnotify >= 0.4 \
    gtk+-2.0 >= 2.2     \
    glib-2.0 >= 2.2     \
    libsexy >= 0.1  \
    libebook-1.2 >= 2.22
)

AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

AC_OUTPUT([
Makefile 
src/Makefile
src/config/Makefile
src/dbus/Makefile
src/contacts/Makefile
src/contacts/addressbook/Makefile
pixmaps/Makefile
sflphone.desktop
])


