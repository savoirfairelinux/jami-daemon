AC_INIT([SFLphone],[0.8],[sflphoneteam@savoirfairelinux.com],[sflphone])
AM_CONFIG_HEADER(config.h)

PACKAGE=SFLphone
VERSION=0.8

AM_INIT_AUTOMAKE($PACKAGE,$VERSION)

AC_ARG_WITH(debug,
		AS_HELP_STRING(
			[--with-debug],
			[Set 'full' to enable debugging information @<:@default=no@:>@]
		),
		[with_debug=${withval}],
		[with_debug=no]
	)
if test "x$with_debug" = "xfull" -o "x$with_debug" = "xyes"; then
	CFLAGS="$CFLAGS -g -DDEBUG -Wall -Werror "
fi

AC_PROG_CC
AC_PROG_INSTALL
AC_STDC_HEADERS

PKG_CHECK_MODULES(DEPS, \
    dbus-glib-1 >= 0.35 \
    gtk+-2.0 >= 2.2     \
    glib-2.0 >= 2.2     \
    gtkglext-1.0 >= 1.0.0 \
)


AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

AC_OUTPUT([
Makefile 
src/Makefile 
src/po/Makefile 
pixmaps/Makefile
sflphone.desktop
])


