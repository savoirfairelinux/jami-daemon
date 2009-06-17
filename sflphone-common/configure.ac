dnl SFLPhone - configure.ac for automake 1.9 and autoconf 2.59
dnl
dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT([SFLphone],[0.9.5~beta],[sflphoneteam@savoirfairelinux.com],[sflphone])
AC_COPYRIGHT([[Copyright (c) Savoir-Faire Linux 2004-2009]])
AC_REVISION([$Revision$])

AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER([config.h])

dnl Use this variable in the program
AC_SUBST(PACKAGE_VERSION)

AM_PROG_LIBTOOL


dnl Rewrite name of programs to be installed
dnl AC_ARG_PROGRAM

dnl Compute canonical system name
dnl AC_CANONICAL_TARGET

dnl Improve make variable MAKE
AC_PROG_MAKE_SET

dnl Where to find configure files
dnl AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])

dnl What to generate
dnl AC_CONFIG_FILES([stamp-h], [echo timestamp > stamp-h])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([libs/Makefile \
  libs/stund/Makefile \
  libs/utilspp/Makefile \
  libs/utilspp/functor/Makefile \
  libs/utilspp/singleton/Makefile \
  libs/dbus-c++/Makefile \
  libs/dbus-c++/src/Makefile \
  libs/dbus-c++/tools/Makefile \
  libs/dbus-c++/data/Makefile \
  libs/iax2/Makefile])
  
AC_CONFIG_FILES([src/Makefile \
  src/audio/Makefile \
  src/audio/codecs/Makefile
  src/audio/codecs/ilbc/Makefile \
  src/config/Makefile \
  src/dbus/Makefile \
  src/plug-in/audiorecorder/Makefile \
  src/plug-in/Makefile \
  src/plug-in/test/Makefile \
  src/hooks/Makefile \
  src/history/Makefile]) 
  
  dnl Unitary test section
AC_CONFIG_FILES([test/Makefile])


AC_CONFIG_FILES([po/Makefile \
  ringtones/Makefile])

AC_CONFIG_FILES([man/Makefile])

AC_CONFIG_FILES([doc/Makefile \
  doc/doxygen/Makefile])
  
dnl the file stamp-h.in should be there before (instead of AC_CONFIG_HEADERS(config.h))
dnl AM_CONFIG_HEADER(config.h)

dnl Settings need Standard C++ Library
LIBS="$LIBS -lstdc++"

dnl Solaris pkgadd support definitions
PKGADD_PKG="SFLphone"
PKGADD_NAME="SFLphone - a SIP client and daemon"
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
CXXFLAGS="$CXXFLAGS $USE_EXCEPTIONS $USER_INCLUDES -O0 -g -Wno-return-type"

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

dnl ********************************
dnl Check for needed functions
dnl ********************************
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

dnl *****************************************
dnl Check for header files
dnl *****************************************
SFLPHONE_LIBS="$SFLPHONE_LIBS"

dnl Check for GNU ccRTP
PKG_PROG_PKG_CONFIG

#Trying to set PJSIP using pkg-config
#PKG_CHECK_MODULES(SIP, libpj-sfl, have_libpj=true, have_libpj=false)
#if test "x${have_libpj}" = "xfalse" ; then
 #   AC_MSG_ERROR([PJSIP not found. http://www.pjsip.org/download.htm])
#fi
#AC_SUBST(SIP_CFLAGS)

dnl Check for uuid development package - name: uuid-dev 
UUID_MIN_VERSION=1.0
PKG_CHECK_MODULES(UUID, uuid >= ${UUID_MIN_VERSION})
AC_SUBST(UUID_CFLAGS)
AC_SUBST(UUID_LIBS)

dnl Check for alsa development package - name: libasound2-dev 
LIBASOUND2_MIN_VERSION=1.0
PKG_CHECK_MODULES(ALSA, alsa >= ${LIBASOUND2_MIN_VERSION})
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)

dnl Check for pulseaudio development package - name: libpulse-dev 
LIBPULSE_MIN_VERSION=0.9.6
PKG_CHECK_MODULES(PULSEAUDIO, libpulse >= ${LIBPULSE_MIN_VERSION})
AC_SUBST(PULSEAUDIO_LIBS)
AC_SUBST(PULSEAUDIO_CFLAGS)

dnl Check for the samplerate development package - name: libsamplerate0-dev
LIBSAMPLERATE_MIN_VERSION=0.1.2
PKG_CHECK_MODULES(SAMPLERATE, samplerate >= ${LIBSAMPLERATE_MIN_VERSION})
AC_SUBST(SAMPLERATE_LIBS)
AC_SUBST(SAMPLERATE_CFLAGS)

LIBCCGNU2_MIN_VERSION=1.3.1
PKG_CHECK_MODULES(CCGNU2, libccgnu2 >= ${LIBCCGNU2_MIN_VERSION})
AC_SUBST(CCGNU2_LIBS)
AC_SUBST(CCGNU2_FLAGS)

LIBCCEXT2_MIN_VERSION=1.3.1
PKG_CHECK_MODULES(CCEXT2, libccext2 >= ${LIBCCEXT2_MIN_VERSION})
AC_SUBST(CCEXT2_LIBS)
AC_SUBST(CCEXT2_CFLAGS)

LIBCCRT_MIN_VERSION=1.3.0
PKG_CHECK_MODULES(CCRTP, libccrtp1 >= ${LIBCCRT_MIN_VERSION})
AC_SUBST(CCRTP_LIBS)
AC_SUBST(CCRTP_CFLAGS)

dnl DBus-C++ detection (used to be in library own build system)
DBUS_REQUIRED_VERSION=0.60
PKG_CHECK_MODULES(dbus, [dbus-1 >= $DBUS_REQUIRED_VERSION],,
	AC_MSG_ERROR([You need the DBus libraries (version 0.6 or better)]
	[http://gitorious.org/dbus-cplusplus])
)
AC_SUBST(dbus_CFLAGS)
AC_SUBST(dbus_LIBS)

DBUS_API_STABLE_VERSION=1.0.0
PKG_CHECK_EXISTS([dbus-1 < $DBUS_API_STABLE_VERSION],
	[AC_DEFINE(DBUS_API_SUBJECT_TO_CHANGE, , [unstable DBus])]
)

DBUS_THREADS_INIT_DEFAULT_VERSION=0.93
PKG_CHECK_EXISTS([dbus-1 >= $DBUS_THREADS_INIT_DEFAULT_VERSION],
	[AC_DEFINE(DBUS_HAS_THREADS_INIT_DEFAULT, , [dbus_threads_init_default (needs DBus >= 0.93)])]
)

DBUS_RECURSIVE_MUTEX_VERSION=0.95
PKG_CHECK_EXISTS([dbus-1 >= $DBUS_RECURSIVE_MUTEX_VERSION],
	[AC_DEFINE(DBUS_HAS_RECURSIVE_MUTEX, , [DBus supports recursive mutexes (needs DBus >= 0.95)])]
)

#AC_ARG_ENABLE(dbus-debug,
#	AS_HELP_STRING([--enable-dbus-debug],
#		[enable debugging support]),
#	[enable_debug=$enableval],
#	[enable_debug=no]
#)

#if test "$enable_debug" = "yes" ; then
#	CXXFLAGS="$CXXFLAGS -Wall -ggdb -O0"
#	AC_DEFINE(DEBUG, 1, [Define to enable debug build])
#else
#	CXXFLAGS="$CXXFLAGS -Wall -O3"
#fi


if test "$enable_glib" = "yes" ; then
PKG_CHECK_MODULES([glib], glib-2.0)
AC_SUBST(glib_CFLAGS)
AC_SUBST(glib_LIBS)
AM_CONDITIONAL(ENABLE_GLIB, test 1 = 1)
PKG_CHECK_MODULES([gtkmm], gtkmm-2.4,
	AM_CONDITIONAL(HAVE_GTKMM, test 1 = 1),
	AM_CONDITIONAL(HAVE_GTKMM, test 0 = 1)
)
AC_SUBST(gtkmm_CFLAGS)
AC_SUBST(gtkmm_LIBS)
else
AM_CONDITIONAL(ENABLE_GLIB, test 0 = 1)
AM_CONDITIONAL(HAVE_GTKMM, test 0 = 1)
fi

AC_CHECK_LIB([expat], XML_ParserCreate_MM,
   	[AC_CHECK_HEADERS(expat.h, have_expat=true, have_expat=false)],
	have_expat=false)

if ! $have_expat; then
	AC_MSG_ERROR([You need the eXpat xml parser]
		[http://expat.sourceforge.net/])
fi

xml_CFLAGS=
xml_LIBS=-lexpat

AC_SUBST(xml_CFLAGS)
AC_SUBST(xml_LIBS)

AC_CHECK_LIB([pthread], pthread_create,
	[AC_CHECK_HEADERS(pthread.h, have_pthread=true, have_pthread=false)],
	have_pthread=false)

if test "$have_pthread" = "false"; then
	AC_MSG_ERROR([You need the POSIX Thread library (pthreads)])	
fi

# For the tools/, we need libdbus-c++ for the "build" architecture as well

AM_CONDITIONAL(CROSS_COMPILING, test "$cross_compiling" = "yes")

AC_ARG_WITH(build-libdbus-cxx,
	AS_HELP_STRING([--with-build-libdbus-cxx],
	[For cross compilation: path to libdbus-cxx which was compiled for the 'build' system.]),
	[ BUILD_LIBDBUS_CXX_DIR=${withval} ],
	[ BUILD_LIBDBUS_CXX_DIR="\$(top_builddir)" ]
)
AC_SUBST(BUILD_LIBDBUS_CXX_DIR)

dnl Check for libcppunit-dev
CPPUNIT_MIN_VERSION=1.12
PKG_CHECK_MODULES(CPPUNIT, cppunit >= ${CPPUNIT_MIN_VERSION}, AM_CONDITIONAL(BUILD_TEST, test 1 = 1 ), AM_CONDITIONAL(BUILD_TEST, test 0 = 1 ))
AC_SUBST(CPPUNIT_LIBS)

# check for libgsm1 (doesn't use pkg-config)
dnl Check for libgsm
AC_ARG_WITH([gsm],
  [AS_HELP_STRING([--without-gsm],
    [disable support for gsm codec])],
  [],
  [with_gsm=yes])

LIBGSM=
AS_IF([test "x$with_gsm" != xno],
  [AC_CHECK_HEADER([gsm/gsm.h], , AC_MSG_FAILURE([Unable to find the libgsm1 headers (you may need to install the dev package).  You may use --without-gsm to compile without gsm codec support.]))]
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

AS_IF([test "x$with_speex" != xno],
  [AC_CHECK_HEADER([speex/speex.h], , AC_MSG_FAILURE([Unable to find the libspeex headers (you may need to install the dev package).  You may use --without-speex to compile without speex codec support.]))]
  [AC_CHECK_HEADER([speex/speex_preprocess.h], , AC_MSG_FAILURE([Unable to find the libspeexdsp headers (you may need to install the libspeexdsp-dev package).  You may use --without-speex to compile without speex codec support.]))]
  [AC_CHECK_LIB([speex], [speex_decode_int],
    [],
    [AC_MSG_FAILURE(
       [libspeex link test failed.   You may use --without-speex to compile without speex codec support.])]
       )
  ]
dnl More advanced check in case the libspeexdsp is not installed
AC_SEARCH_LIBS(speex_preprocess_run, speexdsp, HAVE_SPEEXDSP="yes", HAVE_SPEEXDSP="no", [])
)

AC_DEFINE([HAVE_SPEEX], test "x$with_speex" = "xyes", [Define if you have libspeex])
AM_CONDITIONAL(BUILD_SPEEX, test "x$with_speex" = "xyes" )
AM_CONDITIONAL(ENABLE_SPEEXDSP, test  $HAVE_SPEEXDSP = yes)

dnl Check for celt. Compile it only if a recent enough version is found
LIBCELT_MIN_VERSION=0.5.1
PKG_CHECK_MODULES(CELT, celt >= ${LIBCELT_MIN_VERSION}, AM_CONDITIONAL(BUILD_CELT, test 1 = 1 ), AM_CONDITIONAL(BUILD_CELT, test 0 = 1 ))
    
dnl Check for ilbc support
AC_ARG_WITH([ilbc],
  [AS_HELP_STRING([--without-ilbc],
    [disable support for the ilbc codec])],
  [],
  [with_ilbc=no])

AM_CONDITIONAL(BUILD_ILBC, test "x$with_ilbc" = "xyes" )

dnl Check for IAX
AC_ARG_WITH([iax2],
  [AS_HELP_STRING([--without-iax2],
    [disable support for the iax2 protocol])],
  [],
  [with_iax2=yes])

AC_DEFINE([HAVE_IAX], test "x$with_iax2" = "xyes", [Define if you have libiax2])
AM_CONDITIONAL(USE_IAX, test "x$with_iax2" = "xyes" )

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

# Go!
AC_SUBST(SFLPHONE_CFLAGS)
AC_SUBST(SFLPHONE_LIBS)
AC_OUTPUT
AC_MSG_NOTICE([[Configuration done! Please make sure that pjsip library (libs/pjproject-1.0.2) has already been compiled. Run `make' to build the software.]])


