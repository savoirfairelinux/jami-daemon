AC_INIT(acinclude.m4)

AM_CONFIG_HEADER(config.h)

dnl figure out the sflphone version
VERSION=`sed -ne 's/^#define SFLPHONED_VERSION "\(.*\)"/\1/p' ${srcdir}/src/global.h`
AC_SUBST(VERSION)

dnl get rid of release number
RPM_VERSION=`echo $VERSION | sed -ne 's/-//p' `
AC_SUBST(RPM_VERSION)

LIBS="$LIBS -lstdc++"

dnl
dnl Solaris pkgadd support definitions
PKGADD_PKG="SFLPhoned"
PKGADD_NAME="SFLPhone - a SIP client and daemon"
PKGADD_VENDOR="http://www.sflphone.org/"
AC_SUBST(PKGADD_PKG)
AC_SUBST(PKGADD_NAME)
AC_SUBST(PKGADD_VENDOR)

AC_LANG_CPLUSPLUS

AC_PROG_CXX
AC_PROG_CC

dnl AC_PATH_QT
dnl KDE_USE_QT(3.3)
AC_CANONICAL_SYSTEM
AC_ARG_PROGRAM
AM_INIT_AUTOMAKE(sflphoned, $VERSION)
AM_DISABLE_LIBRARIES
AC_PROG_LIBTOOL
AC_CHECK_COMPILERS

CXXFLAGS="$CXXFLAGS $USE_EXCEPTIONS"

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
eXosip2/eXosip.h \
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

dnl 2.2.0 is buggy, header corruption
LIBOSIP2_MIN_VERSION=2.2.1
PKG_CHECK_MODULES(libosip2, libosip2 >= ${LIBOSIP2_MIN_VERSION})
SFLPHONE_CXXFLAGS="$SFLPHONE_CXXFLAGS $libosip2_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libosip2_LIBS"

if test $ac_cv_header_eXosip2_eXosip_h = no; then
   AC_MSG_ERROR([*** missing eXosip2/eXosip.h. You need a working eXosip2 installation. See http://www.antisip.com/download/])
fi

AC_CHECK_LIB([eXosip2], [eXosip_init],[libexosip2_LIBS="-leXosip2 "],[
   AC_MSG_ERROR([*** missing eXosip2 library. You need a working eXosip2 installation. See http://www.antisip.com/download/])])

SFLPHONE_LIBS="$SFLPHONE_LIBS $libexosip2_LIBS"

dnl AC_SUBST(LIBQT)

if test $ac_cv_header_portaudio_h = no; then
   AC_MSG_ERROR([*** missing portaudio.h. You need a working PortAudio installation. See http://www.portaudio.com])
fi
AC_CHECK_LIB([portaudio], [Pa_GetDeviceCount], [
   portaudio_LIBS="-lportaudio "
   portaudio_CFLAGS="-DAUDIO_PORTAUDIO "],
   [AC_MSG_ERROR([*** missing portaudio V19 library. You need a working PortAudio installation. See http://www.portaudio.com])])

SFLPHONE_LIBS="$SFLPHONE_LIBS $portaudio_LIBS"
SFLPHONE_CXXFLAGS=="$SFLPHONE_CXXFLAGS $portaudio_CFLAGS"
AC_SUBST(portaudio_CFLAGS)
AC_SUBST(portaudio_LIB)

AC_SUBST(SFLPHONE_CXXFLAGS)
AC_SUBST(SFLPHONE_LIBS)

AC_ARG_ENABLE(zeroconf, [  --disable-zeroconf	don't require libdns_sd (browsing and publishing DNS-SD services will not be possible) ],  with_zeroconf=$enableval, with_zeroconf=yes)
if test "$with_zeroconf" = "yes"; then
AC_MSG_CHECKING(for DNS-SD support)
save_dnssdtest_LIBS="$LIBS"
save_dnssdtest_LDFLAGS="$LDFLAGS"
save_dnssdtest_CPPFLAGS="$CPPFLAGS"
LDFLAGS="$all_libraries $LDFLAGS"
CPPFLAGS="$CPPFLAGS $all_includes"
case $host_os in
  darwin*) LIBS="" ;;
  *) LIBS="-ldns_sd" ;;
esac
have_libdns_sd="no"
AC_TRY_LINK(	[
			#include <dns_sd.h>
		],[
			DNSServiceRefDeallocate( (DNSServiceRef) 0);
			TXTRecordDeallocate( (TXTRecordRef*) 0);
		],[
			AC_DEFINE(HAVE_DNSSD,1,[Define if dns-sd is available])
			case $host_os in
				darwin*) LIB_DNSSD="" ;;
				*) LIB_DNSSD="-ldns_sd" ;;
			esac
			have_libdns_sd="yes"
			AC_MSG_RESULT(yes)
		],[
			AC_MSG_RESULT(no)
                        LIB_DNSSD=""
])
CPPFLAGS=$save_dnssdtest_CPPFLAGS
LDFLAGS=$save_dnssdtest_LDFLAGS
LIBS=$save_dnssdtest_LIBS
fi
AC_SUBST(LIB_DNSSD)
AM_CONDITIONAL(USE_ZEROCONF, test "$have_libdns_sd" = "yes")	

dnl AC_CONFIG_FILES(
AC_OUTPUT(
sflphone.spec \
sflphoned-fedora.spec \
Makefile \
src/Makefile \
src/audio/Makefile \
src/audio/gsm/Makefile \
src/audio/pacpp/Makefile \
src/audio/pacpp/include/Makefile \
src/audio/pacpp/include/portaudiocpp/Makefile \
src/audio/pacpp/source/Makefile \
src/audio/pacpp/source/portaudiocpp/Makefile \
src/config/Makefile \
src/gui/Makefile \
src/gui/server/Makefile \
src/zeroconf/Makefile \
utilspp/Makefile \
utilspp/functor/Makefile \
utilspp/singleton/Makefile \
stund/Makefile \
ringtones/Makefile \
)
