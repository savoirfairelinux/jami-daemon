AC_INIT([sflphone],[1.4.1],[sflphoneteam@savoirfairelinux.com],[sflphone-plugins])
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE
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

AC_PROG_CC_C99
dnl For target specific flags
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_HEADER_STDC
LT_PREREQ([2.2])
LT_INIT

MIGRATION_FLAGS="-DE_BOOK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGTK_DISABLE_SINGLE_INCLUDES"
if test "x$with_debug" = "xfull" -o "x$with_debug" = "xyes"; then
    CFLAGS="-g -O0 -DDEBUG -Wall -Wextra $MIGRATION_FLAGS"
else
    CFLAGS="$CFLAGS -g -Wall -Wextra -Wshadow $MIGRATION_FLAGS"
fi

PKG_CHECK_MODULES(GTK, gtk+-3.0)
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.2)
PKG_CHECK_MODULES(LIBEBOOK, libebook-1.2 >= 1.4)
PKG_CHECK_MODULES([LIBEDATASERVER], [libedataserver-1.2 >= 2.32],
        [AC_DEFINE([LIBEDATASERVER_VERSION_2_32], [], [Using libedataserver-1.2 version 2.32 or higher])],
        [PKG_CHECK_MODULES([LIBEDATASERVER], [libedataserver-1.2 >= 1.4])]
        )

AC_CONFIG_FILES([
Makefile
addressbook/evolution/Makefile
addressbook/Makefile
addressbook/test/Makefile
])
AC_OUTPUT

