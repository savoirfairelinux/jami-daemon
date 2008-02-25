dnl SFLPhone - configure.ac for automake 1.9 and autoconf 2.59
dnl
dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT([SFLPhone],[0.7],[sflphoneteam@savoirfairelinux.com],[sflphone])
AC_COPYRIGHT([[Copyright (c) Savoir-Faire Linux 2004-2008]])
AC_REVISION([$Revision$])

dnl Compute canonical system name
AC_CANONICAL_TARGET

dnl Rewrite name of programs to be installed
AC_ARG_PROGRAM

dnl Automake options
AM_INIT_AUTOMAKE([-Wall -Wno-portability])

dnl Improve make variable MAKE
AC_PROG_MAKE_SET

dnl Where to find configure files
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])

dnl What to generate
dnl AC_CONFIG_FILES([stamp-h], [echo timestamp > stamp-h])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([libs/Makefile \
  libs/taxidermy/Makefile \
  libs/stund/Makefile \
  libs/utilspp/Makefile \
  libs/utilspp/functor/Makefile \
  libs/utilspp/singleton/Makefile])
  
AC_CONFIG_FILES([src/Makefile \
  src/sflphone \
  src/audio/Makefile \
  src/audio/codecs/Makefile
  src/audio/codecs/ilbc/Makefile \
  src/config/Makefile \
  src/dbus/Makefile \
  src/zeroconf/Makefile])
  
AC_CONFIG_FILES([skins/Makefile \
  skins/metal/Makefile \
  skins/gmetal/Makefile \
  ringtones/Makefile])

AC_CONFIG_FILES([doc/Makefile \
  doc/doxygen/Makefile])
  
AC_CONFIG_FILES([platform/debian/changelog \
  platform/rpm/sflphone.spec \
  platform/fedora/sflphone.spec \
  platform/fedora/sflphone-fc6.spec])
  
dnl the file stamp-h.in should be there before (instead of AC_CONFIG_HEADERS(config.h))
AM_CONFIG_HEADER(config.h)

dnl Get project version
VERSION=`sed -ne 's/^#define SFLPHONED_VERSION "\(.*\)"/\1/p' ${srcdir}/src/global.h`

dnl Get rid of release number
RPM_VERSION=`echo $VERSION | cut -d- -f1`
AC_SUBST(RPM_VERSION)
RPM_RELEASE=`echo $VERSION | sed -ne 's/.*-\(.*\)/\1/p' `
AC_SUBST(RPM_RELEASE)
VERSION=`echo $VERSION | cut -d- -f1`
AC_SUBST(VERSION)

dnl Settings need Standard C++ Library
LIBS="$LIBS -lstdc++"

dnl Solaris pkgadd support definitions
PKGADD_PKG="SFLPhone"
PKGADD_NAME="SFLPhone - a SIP client and daemon"
PKGADD_VENDOR="http://www.sflphone.org/"
AC_SUBST(PKGADD_PKG)
AC_SUBST(PKGADD_NAME)
AC_SUBST(PKGADD_VENDOR)

dnl Check for programs
AC_PROG_CC
SFL_CXX_WITH_DEBUG
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
dnl will have to remove this
AC_PROG_LIBTOOL

dnl Define C++ as default language
AC_LANG(C++)

dnl Setup C++ compiler flags
CXXFLAGS="$CXXFLAGS $USE_EXCEPTIONS $USER_INCLUDES"

dnl Check for header files
AC_FUNC_ALLOCA
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h libintl.h limits.h malloc.h \
  memory.h netdb.h netinet/in.h stdlib.h string.h strings.h \
  sys/ioctl.h sys/socket.h sys/time.h unistd.h utime.h ostream])

dnl Check for typedefs, structures, and compiler characteristics
AC_HEADER_STAT
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_C_VOLATILE
AC_CHECK_TYPES([ptrdiff_t])

dnl Check for library functions
AC_FUNC_CHOWN
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero floor gethostbyname gethrtime gettimeofday \
  inet_ntoa memset mkdir pathconf pow regcomp select setlocale socket \
  strchr strdup strerror strrchr strstr strtol utime])

dnl Check for portaudio
dnl PORTAUDIO_SETUP

dnl Check for exosip2
LP_CHECK_EXOSIP2
SFLPHONE_LIBS="$SFLPHONE_LIBS $EXOSIP_LIBS"


dnl Check for samplerate
dnl AC_CHECK_HEADER([samplerate.h], [
dnl  AC_CHECK_LIB(samplerate, src_simple, [with_samplerate=yes], [with_samplerate=no]) 
dnl  ], [ with_samplerate=no ]
dnl )
dnl AM_CONDITIONAL(USE_SAMPLERATE, test x$with_samplerate = xyes)

dnl Check for GNU ccRTP
PKG_PROG_PKG_CONFIG

LIBOSIP2_OSIPLISTNOPOINTER_MIN_VERSION=3.0.0
PKG_CHECK_MODULES([libosip2], [libosip2 >= ${LIBOSIP2_OSIPLISTNOPOINTER_MIN_VERSION}],
	[LIBOSIP2_CFLAGS=""], [LIBOSIP2_CFLAGS="-DLIBOSIP2_WITHPOINTER"])
SIP_CFLAGS="$SIP_CFLAGS $LIBOSIP2_CFLAGS";
AC_SUBST(SIP_CFLAGS)

dnl LIBGSM1_MIN_VERSION=1.10
dnl PKG_CHECK_MODULES(gsm, gsm >= ${LIBGSM1_MIN_VERSION})
dnl SFLPHONE_CFLAGS="$SFLPHONE_CFLAGS $libgsm1_CFLAGS"
dnl SFLPHONE_LIBS="$SFLPHONE_LIBS $libgsm1_LIBS"

LIBSAMPLERATE_MIN_VERSION=0.1.1
PKG_CHECK_MODULES(samplerate, samplerate >= ${LIBSAMPLERATE_MIN_VERSION})
SFLPHONE_CFLAGS="$SFLPHONE_CFLAGS $samplerate_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $samplerate_LIBS"

LIBCCGNU2_MIN_VERSION=1.3.1
PKG_CHECK_MODULES(libccgnu2, libccgnu2 >= ${LIBCCGNU2_MIN_VERSION})
SFLPHONE_CFLAGS="$SFLPHONE_CFLAGS $libccgnu2_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libccgnu2_LIBS"

LIBCCEXT2_MIN_VERSION=1.3.1
PKG_CHECK_MODULES(libccext2, libccext2 >= ${LIBCCEXT2_MIN_VERSION})
SFLPHONE_CFLAGS="$SFLPHONE_CFLAGS $libccext2_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libccext2_LIBS"

LIBCCRT_MIN_VERSION=1.3.0
PKG_CHECK_MODULES(libccrtp1, libccrtp1 >= ${LIBCCRT_MIN_VERSION})
SFLPHONE_CFLAGS="$SFLPHONE_CFLAGS $libccrtp1_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libccrtp1_LIBS"

LIBPORTAUDIO_MIN_VERSION=19
PKG_CHECK_MODULES(libportaudio, portaudio-2.0 >= ${LIBPORTAUDIO_MIN_VERSION})
SFLPHONE_CFLAGS="$SFLPHONE_CFLAGS $libportaudio_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libportaudio_LIBS"

LIBPORTAUDIOCPP_MIN_VERSION=12
PKG_CHECK_MODULES(libportaudiocpp, portaudiocpp >= ${LIBPORTAUDIOCPP_MIN_VERSION})
SFLPHONE_CFLAGS="$SFLPHONE_CFLAGS $libportaudiocpp_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libportaudiocpp_LIBS"

LIBDBUSCPP_MIN_VERSION=0.5
PKG_CHECK_MODULES(libdbuscpp, dbus-c++-1 >= ${LIBDBUSCPP_MIN_VERSION})
SFLPHONE_CFLAGS="$SFLPHONE_CFLAGS $libdbuscpp_CFLAGS"
SFLPHONE_LIBS="$SFLPHONE_LIBS $libdbuscpp_LIBS"

# check for libboost (doesn't use pkg-config)
AC_CHECK_HEADER(boost/tokenizer.hpp, , AC_MSG_ERROR("Unable to find the libboost tokenizer headers; you may need to install the libboost-dev package" ) )

# check for libgsm1 (doesn't use pkg-config)
dnl Check for libspeex
AC_ARG_WITH([gsm],
  [AS_HELP_STRING([--without-gsm],
    [disable support for gsm codec])],
  [],
  [with_gsm=yes])

LIBGSM=
AS_IF([test "x$with_gsm" != xno],
  [AC_CHECK_HEADER([gsm.h], , AC_MSG_FAILURE([Unable to find the libgsm1 headers (you may need to install the dev package).  You may use --without-gsm to compile without gsm codec support.]))]
  [AC_CHECK_LIB([gsm], [gsm_decode],
    [],
    [AC_MSG_FAILURE(
       [libgsm link test failed.   You may use --without-gsm to compile without gsm codec support.])]
       )
   ]
   )
    
AC_DEFINE([HAVE_GSM], test "x$with_gsm" = "xyes", [Define if you have libgsm])
AM_CONDITIONAL(BUILD_GSM, test "x$with_gsm" = "xyes" )

dnl Check for libspeex
AC_ARG_WITH([speex],
  [AS_HELP_STRING([--without-speex],
    [disable support for speex codec])],
  [],
  [with_speex=yes])

LIBSPEEX=
AS_IF([test "x$with_speex" != xno],
  [AC_CHECK_HEADER([speex/speex.h], , AC_MSG_FAILURE([Unable to find the libspeex headers (you may need to install the dev package).  You may use --without-speex to compile without speex codec support.]))]
  [AC_CHECK_LIB([speex], [speex_decode_int],
    [],
    [AC_MSG_FAILURE(
       [libspeex link test failed.   You may use --without-speex to compile without speex codec support.])]
       )
   ]
   )
    
AC_DEFINE([HAVE_SPEEX], test "x$with_speex" = "xyes", [Define if you have libspeex])
AM_CONDITIONAL(BUILD_SPEEX, test "x$with_speex" = "xyes" )


dnl Check for IAX
AC_ARG_WITH([iax2],
  [AS_HELP_STRING([--without-iax2],
    [disable support for the iax2 protocol])],
  [],
  [with_iax2=yes])

LIBIAX2=
AS_IF([test "x$with_iax2" != xno],
  [AC_CHECK_HEADER([iax/iax.h], , AC_MSG_FAILURE([Unable to find the libiax2 headers (you may need to install the dev package).    You may use --without-iax2 to compile without iax2 protocol support.]))]
  [AC_CHECK_LIB([iax], [iax_init],
    [],
    [AC_MSG_FAILURE(
       [libiax2 link test failed.   You may use --without-iax2 to compile without iax2 protocol support.])]
       )
   ]
   )
    
AC_DEFINE([HAVE_IAX], test "x$with_iax2" = "xyes", [Define if you have libiax2])
AM_CONDITIONAL(USE_IAX, test "x$with_iax2" = "xyes" )

dnl Check for readline
GNUPG_CHECK_READLINE

dnl Check for zeroconf (from apple)
AC_ARG_ENABLE(zeroconf, 
  AS_HELP_STRING(
    [--disable-zeroconf],
    [disables libdns_sd (browsing and publishing DNS-SD services will not be possible) @<:@default=no@:>@]
  ),  
  [without_zeroconf=$enableval], 
  [without_zeroconf="no"]
)
if test "x$without_zeroconf" = "xno"; then
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
  AC_LINK_IFELSE(
    [AC_LANG_SOURCE([[
        #include <dns_sd.h>
      ]], [[
        DNSServiceRefDeallocate( (DNSServiceRef) 0);
        TXTRecordDeallocate( (TXTRecordRef*) 0);
      ]])
    ],[
      AC_DEFINE(HAVE_DNSSD, 1, [Define if dns-sd is available])
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


 
# check for doxygen, mostly stolen from http://log4cpp.sourceforge.net/
# ----------------------------------------------------------------------------
AC_DEFUN([BB_ENABLE_DOXYGEN],
[
AC_ARG_ENABLE(doxygen, [  --enable-doxygen        enable documentation generation with doxygen (auto)])
AC_ARG_ENABLE(dot, [  --enable-dot            use 'dot' to generate graphs in doxygen (auto)])              
AC_ARG_ENABLE(html-docs, [  --enable-html-docs      enable HTML generation with doxygen (yes)], [], [ enable_html_docs=yes])              
AC_ARG_ENABLE(latex-docs, [  --enable-latex-docs     enable LaTeX documentation generation with doxygen (no)], [], [ enable_latex_docs=no])              
if test "x$enable_doxygen" = xno; then
        enable_doc=no
else 
        AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
        if test x$DOXYGEN = x; then
                if test "x$enable_doxygen" = xyes; then
                        AC_MSG_ERROR([could not find doxygen])
                fi
                enable_doc=no
        else
                enable_doc=yes
                AC_PATH_PROG(DOT, dot, , $PATH)
        fi
fi
AM_CONDITIONAL(DOC, test x$enable_doc = xyes)

if test x$DOT = x; then
        if test "x$enable_dot" = xyes; then
                AC_MSG_ERROR([could not find dot])
        fi
        enable_dot=no
else
        enable_dot=yes
fi
AM_CONDITIONAL(ENABLE_DOXYGEN, test x$enable_doc = xtrue)
AC_SUBST(enable_dot)
AC_SUBST(enable_html_docs)
AC_SUBST(enable_latex_docs)
])
# Acutally perform the doxygen check
BB_ENABLE_DOXYGEN

dnl Datadir directory
sflphone_datadir=$datadir/sflphone
AC_SUBST(sflphone_datadir)

dnl Dbus Service activation part
DBUS_SERVICES_DIR="$datadir/dbus-1/services"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [Where services dir for DBUS is])


dnl QT interface
AC_MSG_CHECKING([whether to enable sflphoneqt])
AC_ARG_ENABLE(sflphoneqt,
   AS_HELP_STRING(
     [--enable-sflphoneqt],
     [enable sflphone-qt compilation @<:@default=no@:>@]
   ),
   [with_sflphoneqt=$enableval],
   [with_sflphoneqt=no]
)
AM_CONDITIONAL(WITH_QT, test "x$with_sflphoneqt" = "xyes")
AM_CONDITIONAL(include_x11, test x$with_sflphoneqt = "xyes")
if test "x$with_sflphoneqt" = "xno"; then
  AC_MSG_RESULT(no)
else
  AC_MSG_RESULT(yes)
  gw_CHECK_QT
fi

# Go!
AC_SUBST(SFLPHONE_CFLAGS)
AC_SUBST(SFLPHONE_LIBS)
AC_OUTPUT
AC_MSG_NOTICE([[Configuration done! Run `make' to build the software.]])
