AC_INIT([SFLphone],[0.9.13],[sflphoneteam@savoirfairelinux.com],[sflphone-plugins])
AC_CONFIG_HEADERS([config.h])

GNOME_DOC_INIT

PACKAGE=SFLphone
VERSION=`cat VERSION`

AM_INIT_AUTOMAKE($PACKAGE,$VERSION)
AC_CONFIG_MACRO_DIR([m4])

dnl Silent build by default. Use make V=1 to increase verbosity
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([no])])

AC_ARG_WITH(debug,
		AS_HELP_STRING(
			[--with-debug],
			[Set 'full' to enable debugging information @<:@default=no@:>@]
		),
		[with_debug=${withval}],
		[with_debug=no]
	)
if test "x$with_debug" = "xfull" -o "x$with_debug" = "xyes"; then
	CFLAGS="-g -O0 -DDEBUG -Wall -Wextra"
else
	CFLAGS="$CFLAGS -Wall -Wextra"
fi

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC
LT_INIT

dnl GCONF utilities
AM_GCONF_SOURCE_2

dnl uninstalled gsr ui dir
AC_DEFINE_UNQUOTED(SFLPHONE_UIDIR_UNINSTALLED, "`pwd`/src/",
  [path to uninstalled SFLphone UI dir])

PKG_CHECK_MODULES(SFLGTK, gtk+-2.0 > 2.2)
PKG_CHECK_MODULES(SFLGLIB, glib-2.0 >= 2.2)
PKG_CHECK_MODULES(LIBEBOOK, libebook-1.2 >= 1.4)
PKG_CHECK_MODULES([LIBEDATASERVER], [libedataserver-1.2 >= 2.32], [AC_DEFINE([LIBEDATASERVER_VERSION_2_32], [], [Using libedataserver-1.2 version 2.32 or higher])],[PKG_CHECK_MODULES([LIBEDATASERVER], [libedataserver-1.2 >= 1.4])])

AC_SUBST(SFLGTK_LIBS)
AC_SUBST(SFLGLIB_LIBS)
AC_SUBST(LIBEBOOK_LIBS)
AC_SUBST(LIBEDATASERVER_LIBS)

AC_CONFIG_FILES([
Makefile 
plugin/addressbook/Makefile
plugin/Makefile
])
AC_OUTPUT

