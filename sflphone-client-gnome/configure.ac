AC_INIT([SFLphone],[0.9.13],[sflphoneteam@savoirfairelinux.com],[sflphone-client-gnome])
AC_CONFIG_HEADERS([config.h])

GNOME_DOC_INIT

PACKAGE=SFLphone
VERSION=`cat VERSION`

AM_INIT_AUTOMAKE($PACKAGE,$VERSION)
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
PKG_CHECK_MODULES(LIBEBOOK, libebook-1.2 >= 1.4)
PKG_CHECK_MODULES(LIBEDATASERVER, libedataserver-1.2)
#PKG_CHECK_MODULES(LIBEDATASERVERUI, libedataserverui-1.2)
PKG_CHECK_MODULES(LIBGNOMEUI, libgnomeui-2.0)
PKG_CHECK_MODULES(GNOMEDOCUTILS, gnome-doc-utils)

AC_SUBST(SFLGTK_CFLAGS)
AC_SUBST(SFLGLIB_CFLAGS)
AC_SUBST(WEBKIT_CFLAGS)
AC_SUBST(LIBEBOOK_CFLAGS)
AC_SUBST(LIBEDATASERVER_CFLAGS)
AC_SUBST(LIBEDATASERVERUI_CFLAGS)
AC_SUBST(LIBGNOMEUI_CFLAGS)
AC_SUBST(GNOMEDOCUTILS)

AC_SUBST(SFLGTK_LIBS)
AC_SUBST(SFLGLIB_LIBS)
AC_SUBST(WEBKIT_LIBS)
AC_SUBST(LIBEBOOK_LIBS)
AC_SUBST(LIBEDATASERVER_LIBS)
AC_SUBST(LIBGNOMEUI_LIBS)
AC_SUBST(GNOMEDOCUTILS)


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
src/contacts/addressbook/Makefile
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

