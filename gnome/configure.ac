AC_INIT([sflphone],[0.9.14],[sflphoneteam@savoirfairelinux.com],[sflphone-client-gnome])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

GNOME_DOC_INIT

AC_CONFIG_MACRO_DIR([m4])

dnl Silent build by default. Use make V=1 to increase verbosity
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_ARG_WITH(debug,
		AS_HELP_STRING(
			[--with-debug],
			[Set 'full' to enable debugging information @<:@default=no@:>@]
		),
		[with_debug=${withval}],
		[with_debug=no]
	)
if test "x$with_debug" = "xfull" -o "x$with_debug" = "xyes"; then
	CFLAGS="-g -O0 -Wall -Wextra"
else
	CFLAGS="$CFLAGS -Wall -Wextra"
fi

AC_PROG_CC
AM_PROG_CC_C_O dnl for per target flags, i.e. test_program_CFLAGS
AC_PROG_INSTALL
AC_HEADER_STDC
LT_INIT

dnl GCONF utilities
AM_GCONF_SOURCE_2

dnl uninstalled gsr ui dir
AC_DEFINE_UNQUOTED(SFLPHONE_UIDIR_UNINSTALLED, "`pwd`/src/",
  [path to uninstalled SFLphone UI dir])

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

PKG_CHECK_MODULES(DBUSGLIB, dbus-glib-1 >= 0.75, HAVE_DBUS_G_PROXY_SET_DEFAULT_TIMEOUT=true, HAVE_DBUS_G_PROXY_SET_DEFAULT_TIMEOUT=false);
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify >= 0.7.2], [AC_DEFINE([LIBNOTIFY_VERSION_0_7_2],[],[Using libnotify-0.7.2 or higher])],[PKG_CHECK_MODULES([LIBNOTIFY], [libnotify >= 0.4])])
PKG_CHECK_MODULES(SFLGTK, gtk+-2.0 > 2.2)
PKG_CHECK_MODULES(SFLGLIB, glib-2.0 >= 2.2)
PKG_CHECK_MODULES(WEBKIT, webkit-1.0)
PKG_CHECK_MODULES(LIBGNOMEUI, libgnomeui-2.0)
PKG_CHECK_MODULES(GNOMEDOCUTILS, gnome-doc-utils)


# This macro is defined in check.m4 and tests if check.h and
# libcheck.a are installed in your system. It sets CHECK_CFLAGS and
# CHECK_LIBS accordingly.  
#  AM_PATH_CHECK([MINIMUM-VERSION,
#                [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl AM_PATH_CHECK() DEPRECATED
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

AC_CONFIG_FILES([
Makefile 
src/Makefile
src/config/Makefile
src/dbus/Makefile
src/contacts/Makefile
src/widget/Makefile
src/icons/Makefile
pixmaps/Makefile
webkit/Makefile
sflphone.desktop
tests/Makefile
man/Makefile
po/Makefile
doc/Makefile
])
AC_OUTPUT

