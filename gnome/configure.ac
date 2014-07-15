AC_PREREQ([2.64])

AC_INIT([sflphone],[1.4.1],[sflphoneteam@savoirfairelinux.com],[sflphone-client-gnome])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 -Wall -Wno-portability dist-xz no-define no-dist-gzip subdir-objects])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])

GNOME_MAINTAINER_MODE_DEFINES
GNOME_COMPILE_WARNINGS([maximum])

AC_PROG_CC_C99
AM_PROG_CC_C_O dnl for per target flags, i.e. test_program_CFLAGS
AC_PROG_INSTALL
AC_HEADER_STDC

# init libtool
LT_PREREQ([2.2.6])
LT_INIT(disable-static)

CFLAGS="$CFLAGS -Wall -Wextra -Wshadow"

dnl uninstalled gsr ui dir
AC_DEFINE_UNQUOTED(SFLPHONE_UIDIR_UNINSTALLED, "`pwd`/src/",
  [path to uninstalled SFLphone UI dir])

PKG_CHECK_MODULES(DBUSGLIB, dbus-glib-1 >= 0.75);

PKG_CHECK_MODULES([LIBNOTIFY], [libnotify >= 0.5.0])
if echo "$LIBNOTIFY_LIBS" | grep -q gtk+-x11-2.0; then
    AC_MSG_ERROR([Your libnotify is linked with GTK+2 ! Install libnotify4-dev])
    exit 1
fi

dnl Video is default-enabled
AC_ARG_ENABLE([video], AS_HELP_STRING([--disable-video], [Disable video]))
AS_IF([test "x$enable_video" != "xno"], [
    AC_DEFINE(SFL_VIDEO, [], [Video support enabled])
    PKG_CHECK_MODULES(CLUTTER, clutter-1.0)
    PKG_CHECK_MODULES(CLUTTERGTK, clutter-gtk-1.0)
    ]);

AM_CONDITIONAL(SFL_VIDEO, test "x$enable_video" != "xno")

dnl Presence is a default-enabled feature
AC_ARG_ENABLE([presence],
              AS_HELP_STRING([--disable-presence], [Disable SIP presence support]))

AS_IF([test "x$enable_presence" != "xno"], [
    AC_DEFINE(SFL_PRESENCE, [], [Presence support enabled])
    AM_CONDITIONAL(SFL_PRESENCE, true)],
    AM_CONDITIONAL(SFL_PRESENCE, false))

dnl --enable-debug is the default
AC_ARG_ENABLE([debug], AS_HELP_STRING([--disable-debug],
              [Disable assertions/debug symbols]))

AS_IF([test "x$enable_debug" != "xno"],
      [CFLAGS="$CFLAGS -ggdb"],
      [
       AH_TEMPLATE(NDEBUG, [Define to 1 if debug code should NOT be compiled])
       AH_TEMPLATE(G_DISABLE_ASSERT, [Define to 1 if debug code should NOT be compiled])
       AC_DEFINE(NDEBUG)
       AC_DEFINE(G_DISABLE_ASSERT)])

# Check for gtk+-3.0
PKG_CHECK_MODULES(GTK, gtk+-3.0, [AC_MSG_NOTICE(Using gtk+-3.0)],
                                 [AC_MSG_ERROR(gtk+-3.0 not found)])

AC_SUBST([GETTEXT_PACKAGE], [$PACKAGE_NAME])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [GETTEXT package name])
IT_PROG_INTLTOOL([0.40.0])
AM_GNU_GETTEXT_VERSION([0.18])
AM_GNU_GETTEXT([external])


PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.24)
PKG_CHECK_MODULES(X11, x11)

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4],,
                  AC_MSG_RESULT([check not installed]))

GLIB_GSETTINGS

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
doc/Makefile
data/Makefile
po/Makefile.in
])
AC_OUTPUT

