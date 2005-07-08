AC_INIT(acinclude.m4)

AM_CONFIG_HEADER(config.h)

dnl figure out the sflphone version
VERSION=`sed -ne 's/^#define VERSION "\(.*\)"/\1/p' src/global.h`
AC_SUBST(VERSION)

dnl get rid of release number
RPM_VERSION=`echo $VERSION | sed -ne 's/-//p' `
AC_SUBST(RPM_VERSION)

LIBS="$LIBS -lstdc++"

dnl
dnl Solaris pkgadd support definitions
PKGADD_PKG="SFLPhone"
PKGADD_NAME="SFLPhone - a SIP client"
PKGADD_VENDOR="http://www.sflphone.org"
AC_SUBST(PKGADD_PKG)
AC_SUBST(PKGADD_NAME)
AC_SUBST(PKGADD_VENDOR)

AC_LANG_CPLUSPLUS

AC_PROG_CXX
AC_PROG_CC

AC_PATH_QT
dnl KDE_USE_QT(3.3)
AC_CANONICAL_SYSTEM
AC_ARG_PROGRAM
AM_INIT_AUTOMAKE(sflphone, $VERSION)
AM_DISABLE_LIBRARIES
AC_PROG_LIBTOOL
AC_CHECK_COMPILERS

CXXFLAGS="$CXXFLAGS $USE_EXCEPTIONS "

SFLPHONE_CXXFLAGS=""

case $host in
  *-*-cygwin | *-*-mingw* | *-*-pw32*)
    need_no_undefined=yes
    ;;
  *)
    need_no_undefined=no
    ;;
esac

AM_CONDITIONAL(NO_UNDEFINED, test x$need_no_undefined = xyes)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS( \
ostream \
eXosip/eXosip.h \
portaudio.h \
)

LIBCCGNU2_MIN_VERSION=1.3.1
PKG_CHECK_MODULES(libccgnu2, libccgnu2 >= ${LIBCCGNU2_MIN_VERSION})
SFLPHONE_CXXFLAGS="$SFLPHONE_CXXFLAGS $libccgnu2_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libccgnu2_LIBS"

LIBCCEXT2_MIN_VERSION=1.3.1
PKG_CHECK_MODULES(libccext2, libccext2 >= ${LIBCCEXT2_MIN_VERSION})
SFLPHONE_CXXFLAGS="$SFLPHONE_CXXFLAGS $libccext2_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libccext2_LIBS"

LIBCCRT_MIN_VERSION=1.3.0
PKG_CHECK_MODULES(libccrtp1, libccrtp1 >= ${LIBCCRT_MIN_VERSION})
SFLPHONE_CXXFLAGS="$SFLPHONE_CXXFLAGS $libccrtp1_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libccrtp1_LIBS"

LIBOSIP2_MIN_VERSION=2.2.0
PKG_CHECK_MODULES(libosip2, libosip2 >= ${LIBOSIP2_MIN_VERSION})
SFLPHONE_CXXFLAGS="$SFLPHONE_CXXFLAGS $libosip2_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libosip2_LIBS"

if test $ac_cv_header_eXosip_eXosip_h = no; then
   AC_MSG_ERROR([*** missing eXipsip/eXosip.h. You need a working eXosip installation. See http://savannah.nongnu.org/projects/exosip/])
fi
SFLPHONE_LIBS="$SFLPHONE_LIBS -leXosip"

dnl if test $ac_cv_header_gsm_h = no; then
dnl    AC_MSG_ERROR([*** missing gsm.h. You need a working GSM 06.10 lossy speech compression installation. See http://kbs.cs.tu-berlin.de/~jutta/toast.html])
dnl fi
dnl SFLPHONE_LIBS="$SFLPHONE_LIBS -lgsm"

AC_MSG_CHECKING([whether to enable the maintener code])
AC_ARG_ENABLE(maintener,
[  --enable-maintener      Enable maintener code [default=no]],
[
CXXFLAGS="$CXXFLAGS -DENABLE_MAINTENER"
AC_MSG_RESULT(yes)
maintener="yes"
],
[
AC_MSG_RESULT(no)
maintener="no"
]
)

AM_CONDITIONAL(MAINTENER_CODE, test x$maintener = xyes)



if test $ac_cv_header_portaudio_h = no; then
   AC_MSG_ERROR([*** missing portaudio.h. You need a working PortAudio installation. See http://www.portaudio.com])
fi
SFLPHONE_LIBS="$SFLPHONE_LIBS -lportaudio"
CXXFLAGS="$CXXFLAGS -DAUDIO_PORTAUDIO"


AC_SUBST(SFLPHONE_CXXFLAGS)
AC_SUBST(SFLPHONE_LIBS)
CXXFLAGS="$CXXFLAGS $SFLPHONE_CXXFLAGS"
	

LIBS="$LIBS $SFLPHONE_LIBS $LIBQT"

dnl AC_CONFIG_FILES(
AC_OUTPUT(
sflphone.spec \
Makefile \
src/Makefile \
src/audio/Makefile \
src/audio/gsm/Makefile \
src/audio/pacpp/Makefile \
src/audio/pacpp/include/Makefile \
src/audio/pacpp/include/portaudiocpp/Makefile \
src/audio/pacpp/source/Makefile \
src/audio/pacpp/source/portaudiocpp/Makefile \
src/gui/Makefile \
src/gui/qt/Makefile \
stund/Makefile \
pixmaps/Makefile \
skins/Makefile \
skins/emetal/Makefile \
skins/metal/Makefile \
rings/Makefile \
)
