AC_INIT([sflphone],[1.1.0],[sflphoneteam@savoirfairelinux.com],[sflphone-client-gnome])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

GNOME_DOC_INIT

AC_CONFIG_MACRO_DIR([m4])

dnl Silent build by default. Use make V=1 to increase verbosity
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC_C99
AM_PROG_CC_C_O dnl for per target flags, i.e. test_program_CFLAGS
AC_PROG_INSTALL
AC_HEADER_STDC
LT_INIT

CFLAGS="$CFLAGS -Wall -Wextra -Wshadow"

dnl GCONF utilities
AM_GCONF_SOURCE_2

dnl uninstalled gsr ui dir
AC_DEFINE_UNQUOTED(SFLPHONE_UIDIR_UNINSTALLED, "`pwd`/src/",
  [path to uninstalled SFLphone UI dir])

PKG_CHECK_MODULES(DBUSGLIB, dbus-glib-1 >= 0.75, HAVE_DBUS_G_PROXY_SET_DEFAULT_TIMEOUT=true, HAVE_DBUS_G_PROXY_SET_DEFAULT_TIMEOUT=false);
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify >= 0.5.0])
if echo "$LIBNOTIFY_LIBS" | grep -q gtk+-x11-2.0; then
    AC_MSG_ERROR([Your libnotify is linked with GTK+2 ! Install libnotify4-dev])
    exit 1
fi

dnl Video is default-disabled
AC_ARG_ENABLE([video], AS_HELP_STRING([--enable-video], [Enable video]))
AS_IF([test "x$enable_video" = "xyes"], [
    AC_DEFINE(SFL_VIDEO, [], [Video support enabled])

    PKG_CHECK_MODULES(CLUTTER, clutter-1.0)
    PKG_CHECK_MODULES(CLUTTERGTK, clutter-gtk-1.0)
    ]);

AM_CONDITIONAL(SFL_VIDEO, test "x$enable_video" = "xyes")

PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.24)
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(JAVASCRIPT_CORE_GTK, javascriptcoregtk-3.0, ,
		  AC_MSG_RESULT(javascriptcoregtk not installed))

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4],,
                  AC_MSG_RESULT([check not installed]))

AC_CONFIG_FILES([
Makefile
src/Makefile
src/config/Makefile
src/dbus/Makefile
src/contacts/Makefile
src/widget/Makefile
src/video/Makefile
src/icons/Makefile
src/messaging/Makefile
pixmaps/Makefile
sflphone.desktop
tests/Makefile
man/Makefile
po/Makefile
doc/Makefile
])
AC_OUTPUT

