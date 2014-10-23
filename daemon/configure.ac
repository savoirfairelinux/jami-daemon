dnl SFLPhone - configure.ac for automake 1.9 and autoconf 2.59

dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.65])
AC_INIT([sflphone],[1.4.2],[sflphoneteam@savoirfairelinux.com],[sflphone])

AC_COPYRIGHT([[Copyright (c) Savoir-Faire Linux 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014]])
AC_REVISION([$Revision$])

dnl Where to find configure files
AC_CONFIG_SRCDIR(src/sflphone_api.cpp)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([-Wno-portability])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR]) # required by automake 1.12
AC_CONFIG_HEADERS([config.h])

# Silent build by default. Use make V=1 to increase verbosity
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl Check for programs
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl
dnl  Check the operating system
dnl
HAVE_WIN64="0"
HAVE_OSX="0"

case "${host_os}" in
  "")
    SYS=unknown
    ;;
  linux*)
    SYS=linux
    # Necessary for linking .a to a .so
    LDFLAGS+=" -Wl,-Bsymbolic"
    ;;
  *darwin*)
    SYS=darwin
    AC_DEFINE(HAVE_DARWIN,1,[define if compiling for Apple Darwin OSX])
    have_darwin=yes
    HAVE_OSX="1"
    ;;
  mingw32*)
    SYS=mingw32
    AC_CHECK_TOOL(WINDRES, windres, :)
    AC_CHECK_TOOL(OBJCOPY, objcopy, :)
    AH_TOP([#if defined(_WIN32) && !defined(_WIN32_WINNT)])
    AH_TOP([# define _WIN32_WINNT 0x0502 /* Windows XP SP2 */])
    AH_TOP([#endif])
    AC_DEFINE([_WIN32_IE], 0x0600, [Define to '0x0600' for IE 6.0 (and shell) APIs.])
    AC_DEFINE([_UNICODE], [1], [Define to 1 for Unicode (Wide Chars) APIs.])
    AC_DEFINE([UNICODE], [1], [Define to 1 for Unicode (Wide Chars) APIs.])
    AC_DEFINE([_ISOC99_SOURCE], [1], [Extensions to ISO C89 from ISO C99.])
    AC_DEFINE([_ISOC11_SOURCE], [1], [Extensions to ISO C99 from ISO C11.])
    AC_DEFINE([_POSIX_SOURCE], [1], [IEEE Std 1003.1.])
    AC_DEFINE([_POSIX_C_SOURCE], [200809L], [IEEE Std 1003.1.])
    AC_DEFINE([_XOPEN_SOURCE], [700], [POSIX and XPG 7th edition])
    AC_DEFINE([_XOPEN_SOURCE_EXTENDED], [1], [XPG things and X/Open Unix extensions.])
    AC_DEFINE([_BSD_SOURCE], [1], [ISO C, POSIX, and 4.3BSD things.])
    AC_DEFINE([_SVID_SOURCE], [1], [ISO C, POSIX, and SVID things.])

    LDFLAGS="${LDFLAGS} -Wl,--nxcompat -Wl,--no-seh -Wl,--dynamicbase"
    ac_default_prefix="`pwd`/_win32"
    DESTDIR="`pwd`/_win32/"
    AC_SUBST(WINDOWS_ARCH)
    AC_SUBST(PROGRAMFILES)
    ;;
  *)
    SYS="${host_os}"
    ;;
esac
AM_CONDITIONAL(HAVE_DARWIN,  test "${SYS}" = "darwin")
AM_CONDITIONAL(HAVE_LINUX,   test "${SYS}" = "linux")
AM_CONDITIONAL(HAVE_WIN32,   test "${SYS}" = "mingw32")
AM_CONDITIONAL(HAVE_ANDROID, test "${HAVE_ANDROID}" = "1")
AM_CONDITIONAL(HAVE_OSX,   test "${HAVE_OSX}" = "1")
dnl Android is linux, but a bit different
AS_IF([test "$SYS" = linux],[
    AC_MSG_CHECKING([for an Android system])
    AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
        [[#ifndef __ANDROID__
         # error Not Android
         #endif
        ]],[[;]])
    ],[
      HAVE_ANDROID="1"
      AC_MSG_RESULT([yes])
    ],[
      AC_MSG_RESULT([no])
    ])
])
AM_CONDITIONAL(HAVE_ANDROID, test "${HAVE_ANDROID}" = "1")

dnl override platform specific check for dependent libraries
dnl otherwise libtool linking of shared libraries will
dnl fail on anything other than pass_all.
AC_CACHE_VAL(lt_cv_deplibs_check_method,
    [lt_cv_deplibs_check_method=pass_all])

LT_INIT([dlopen win32-dll shared disable-static])

dnl Define C++ as default language
LT_LANG(C++)

DOLT

lt_cv_deplibs_check_method=pass_all

dnl Check for C++11 support
AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

dnl Check for header files
AC_FUNC_ALLOCA
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h libintl.h limits.h malloc.h memory.h \
                  netdb.h netinet/in.h stdlib.h string.h strings.h \
                  sys/ioctl.h sys/socket.h sys/time.h unistd.h utime.h \
                  ostream getopt.h])

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

PKG_PROG_PKG_CONFIG()

dnl On some OS we need static linking
AS_IF([test -n "${PKG_CONFIG}" ],[
    AS_IF([test "${SYS}" = "mingw32" -o "${SYS}" = "darwin"],[
        PKG_CONFIG="${PKG_CONFIG} --static"
    ])
])

dnl Check if we are using clang
AC_MSG_CHECKING([if compiling with clang])
AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([], [[
#ifndef __clang__
       not clang
#endif
]])],
[CLANG=yes], [CLANG=no])

AC_MSG_RESULT([$CLANG])

dnl
dnl  Check for the contrib directory
dnl
AC_ARG_WITH(contrib,
  [AS_HELP_STRING([--with-contrib[=DIR]],
    [search for 3rd party libraries in DIR/include and DIR/lib])
])
AC_MSG_CHECKING([for 3rd party libraries path])
AS_IF([test -z "${with_contrib}" || test "${with_contrib}" = "yes"], [
  CONTRIB_DIR="${srcdir}/contrib/${host}"
  AS_IF([test ! -d "${CONTRIB_DIR}"], [
    echo "${CONTRIB_DIR} not found" >&AS_MESSAGE_LOG_FD
    CONTRIB_DIR="${srcdir}/contrib/`$CC -dumpmachine`"
    AS_IF([test ! -d "${CONTRIB_DIR}"], [
      echo "${CONTRIB_DIR} not found" >&AS_MESSAGE_LOG_FD
      CONTRIB_DIR=""
      AC_MSG_RESULT([not found])
    ])
  ])
], [
  AS_IF([test "${with_contrib}" != "no"], [
    CONTRIB_DIR="${with_contrib}"
  ], [
    CONTRIB_DIR=""
    AC_MSG_RESULT([disabled])
  ])
])

AS_IF([test -n "${CONTRIB_DIR}"], [
  AS_IF([test -d "${CONTRIB_DIR}/lib"],[
    CONTRIB_DIR=`cd "${CONTRIB_DIR}" && pwd`
  ], [
    echo "${CONTRIB_DIR}/lib not found" >&AS_MESSAGE_LOG_FD
    CONTRIB_DIR=""
    AC_MSG_RESULT([not usable])
  ])
])

AS_IF([test -n "${CONTRIB_DIR}"], [
  AC_MSG_RESULT([${CONTRIB_DIR}])
  export PATH=${CONTRIB_DIR}/bin:$PATH
  CPPFLAGS="${CPPFLAGS} -I${CONTRIB_DIR}/include"
  CFLAGS="${CFLAGS} -I${CONTRIB_DIR}/include"
  CXXFLAGS="${CXXFLAGS} -I${CONTRIB_DIR}/include"
  OBJCFLAGS="${OBJCFLAGS} -I${CONTRIB_DIR}/include"
  export PKG_CONFIG_PATH="${CONTRIB_DIR}/lib/pkgconfig:${CONTRIB_DIR}/lib64/pkgconfig:$PKG_CONFIG_PATH"
  LDFLAGS="${LDFLAGS} -L${CONTRIB_DIR}/lib"

  AS_IF([test "${SYS}" = "darwin"], [
    export LD_LIBRARY_PATH="${CONTRIB_DIR}/lib:$LD_LIBRARY_PATH"
    export DYLD_LIBRARY_PATH="${CONTRIB_DIR}/lib:$DYLD_LIBRARY_PATH"
  ])
], [
  AS_IF([test -n "${with_contrib}" && test "${with_contrib}" != "no"], [
    AC_MSG_ERROR([Third party libraries not found!])
  ])
])
AC_SUBST(CONTRIB_DIR)

dnl Add extras/tools to the PATH
TOOLS_DIR="${srcdir}/extras/tools/build/bin"
AS_IF([test -d "${TOOLS_DIR}"], [
  TOOLS_DIR=`cd "${TOOLS_DIR}" && pwd`
  export PATH="${TOOLS_DIR}":$PATH
])

dnl Check for pjproject
PKG_CHECK_MODULES(PJPROJECT, libpjproject,, AC_MSG_ERROR([Missing pjproject files]))

PKG_CHECK_MODULES([YAMLCPP], [yaml-cpp >= 0.5.1],, AC_MSG_ERROR([yaml-cpp not found]))

if test "${HAVE_ANDROID}" = "1"; then
    dnl Check for OpenSL
    AC_ARG_WITH([opensl],
                AS_HELP_STRING([--without-opensl],
                               [Ignore presence of opensl and disable it]))

    AS_IF([test "x$with_opensl" != "xno"],
          [AC_CHECK_HEADER(["SLES/OpenSLES.h"],
                           [have_opensl=yes], [have_opensl=no])],
          [have_opensl=no])
    AC_DEFINE_UNQUOTED([HAVE_OPENSL],
                       `if test "x$have_opensl" = "xyes"; then echo 1; else echo 0; fi`,
                       [Define if you have OpenSL])
fi
AM_CONDITIONAL([BUILD_OPENSL], test "x$have_opensl" = "xyes")

dnl Check for alsa development package - name: libasound2-dev
ALSA_MIN_VERSION=1.0
AC_ARG_WITH([alsa],
	[AS_HELP_STRING([--without-alsa], [disable support for alsa])],
	[],
	[with_alsa=yes])
AS_IF([test "x$with_alsa" = "xyes"], [
       PKG_CHECK_MODULES(ALSA, alsa >= ${ALSA_MIN_VERSION},, AC_MSG_ERROR([Missing alsa development files]))
	]);

AC_DEFINE_UNQUOTED([HAVE_ALSA], `if test "x$with_alsa" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have alsa])
AM_CONDITIONAL(BUILD_ALSA, test "x$with_alsa" = "xyes")


dnl Check for pulseaudio development package - name: libpulse-dev
LIBPULSE_MIN_VERSION=0.9.15
AC_ARG_WITH([pulse],
    [AS_HELP_STRING([--without-pulse], [disable support for pulseaudio])],
    [],
    [with_pulse=yes])

AS_IF([test "x$with_pulse" = "xyes"], [
       PKG_CHECK_MODULES(PULSEAUDIO, libpulse >= ${LIBPULSE_MIN_VERSION},, AC_MSG_ERROR([Missing pulseaudio development files]))
      ]);

AC_DEFINE_UNQUOTED([HAVE_PULSE], `if test "x$with_pulse" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have pulseaudio])
AM_CONDITIONAL(BUILD_PULSE, test "x$with_pulse" = "xyes")

AC_ARG_WITH([jack],
    AS_HELP_STRING([--without-jack], [Ignore presence of jack and disable it]))

AS_IF([test "x$with_jack" != "xno"],
      [PKG_CHECK_MODULES(JACK, jack, [have_jack=yes], [have_jack=no])],
      [have_jack=no])

AS_IF([test "x$have_jack" = "xyes"],,
      [AS_IF([test "x$with_jack" = "xyes"],
             [AC_MSG_ERROR([jack requested but not found])
      ])
])

AC_DEFINE_UNQUOTED([HAVE_JACK], `if test "x$have_jack" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have jack])
AM_CONDITIONAL(BUILD_JACK, test "x$have_jack" = "xyes")

dnl Check for core audio
AC_ARG_WITH([coreaudio],
    AS_HELP_STRING([--without-coreaudio], [Ignore presence of coreaudio and disable it]))
AC_DEFINE_UNQUOTED([HAVE_COREAUDIO],`if test "x$with_coreaudio" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have core audio])
AM_CONDITIONAL(BUILD_COREAUDIO, test "x$with_coreaudio" = "xyes")

dnl Check for the samplerate development package - name: libsamplerate0-dev
LIBSAMPLERATE_MIN_VERSION=0.1.2
PKG_CHECK_MODULES(SAMPLERATE, samplerate >= ${LIBSAMPLERATE_MIN_VERSION},, AC_MSG_ERROR([Missing libsamplerate development files]))

dnl Check for the sndfile development package - name: libsndfile-dev
PKG_CHECK_MODULES(SNDFILE, sndfile,, AC_MSG_ERROR([Missing sndfile development files]))

dnl Coverage is default-disabled
AC_ARG_ENABLE([coverage], AS_HELP_STRING([--enable-coverage], [Enable coverage]))

AS_IF([test "x$enable_coverage" = "xyes"], [
        CXXFLAGS="${CXXFLAGS} --coverage"
        LDFLAGS="${LDFLAGS} --coverage"])

# DBUSCPP
dnl Check for dbuscpp, the C++ bindings for D-Bus
AC_ARG_WITH([dbus],
    [AS_HELP_STRING([--without-dbus], [disable support for dbus])],
    [],
    [with_dbus=yes])
AS_IF([test "x$with_dbus" = "xyes"], [
       PKG_CHECK_MODULES(DBUSCPP, dbus-c++-1,, AC_MSG_WARN([Missing dbus development files]))
       AC_CONFIG_FILES([bin/dbus/Makefile])

       AC_CHECK_PROG(HAVE_DBUSXML2CPP, dbusxx-xml2cpp, true, false)
       if test "x$HAVE_DBUSXML2CPP" = "xfalse"; then
           AC_MSG_ERROR([dbusxx-xml2cpp not found])
       fi

       AS_IF([test "x$CLANG" = "xyes"], [
             AC_MSG_ERROR([dbus does not compile when clang is used, rerun with --without-dbus])
       ]);

       AM_CONDITIONAL(SFL_DBUS, true)],
       AM_CONDITIONAL(SFL_DBUS, false));


dnl Video is default-enabled
AC_ARG_ENABLE([video], AS_HELP_STRING([--disable-video], [Disable video]))

AS_IF([test "x$enable_video" != "xno"],
      [
        AC_DEFINE(SFL_VIDEO, [], [Video support enabled])
        AM_CONDITIONAL(SFL_VIDEO, true)

        PKG_CHECK_MODULES(LIBAVCODEC, libavcodec >= 53.5.0,, AC_MSG_ERROR([Missing libavcodec development files]))
        LIBAVCODEC_CFLAGS="${LIBAVCODEC_CFLAGS} -D__STDC_CONSTANT_MACROS"

        PKG_CHECK_MODULES(LIBAVFORMAT, libavformat >= 53.2.0,, AC_MSG_ERROR([Missing libavformat development files]))

        PKG_CHECK_MODULES(LIBSWSCALE, libswscale >= 1.1.0,, AC_MSG_ERROR([Missing libswscale development files]))

        PKG_CHECK_MODULES(LIBAVDEVICE, libavdevice >= 53.0.0,, AC_MSG_ERROR([Missing libavdevice development files]))

        PKG_CHECK_MODULES(LIBAVUTIL, libavutil >= 51.0.0,, AC_MSG_ERROR([Missing libavutil development files]))

        PKG_CHECK_MODULES(UDEV, libudev,, AC_MSG_ERROR([Missing libudev development files]))
    ],
    [
     AM_CONDITIONAL(SFL_VIDEO, false)
     ]);

LIBCCRTP_MIN_VERSION=1.3.0
PKG_CHECK_MODULES([CCRTP], [libccrtp] >= ${LIBCCRTP_MIN_VERSION},, [
                   PKG_CHECK_MODULES([CCRTP], [libccrtp1] >= ${LIBCCRTP_MIN_VERSION},, AC_MSG_ERROR([Missing ccrtp development files]))
        ])

dnl Presence is a default-enabled feature
AC_ARG_ENABLE([presence],
              AS_HELP_STRING([--disable-presence], [Disable SIP presence support]))

AS_IF([test "x$enable_presence" != "xno"], [
    AC_DEFINE(SFL_PRESENCE, [], [Presence support enabled])
    AM_CONDITIONAL(SFL_PRESENCE, true)],
    AM_CONDITIONAL(SFL_PRESENCE, false))

dnl TLS support is enabled if it's installed and up to date
AC_ARG_WITH([tls],
  [AS_HELP_STRING([--with-tls],
    [support tls @<:@default=check@:>@])],
  [],
  [with_tls=check])
AS_CASE(["$with_tls"],
  [yes], [PKG_CHECK_MODULES([GNUTLS], [gnutls >= 3.1], [HAVE_GNUTLS=1])],
  [no], [HAVE_GNUTLS=0],
  [PKG_CHECK_MODULES([GNUTLS], [gnutls >= 3.1], [HAVE_GNUTLS=1], [HAVE_GNUTLS=0])])

AC_DEFINE_UNQUOTED([HAVE_TLS], `if test $HAVE_GNUTLS -eq 1; then echo 1; else echo 0; fi`, [Define if you have tls support])
AM_CONDITIONAL(BUILD_TLS, test "$HAVE_GNUTLS" -eq 1)

# ZRTP
# required dependency(ies): libzrtp
dnl Check for libzrtpcpp, a ccRTP extension providing zrtp key exchange
AC_ARG_WITH([zrtp],
    [AS_HELP_STRING([--without-zrtp], [disable support for zrtp])],
    [],
    [with_zrtp=yes])
AS_IF([test "x$with_zrtp" = "xyes"], [
       PKG_CHECK_MODULES([ZRTPCPP], libzrtpcpp >= 2.3.0, [with_zrtp_configure=yes],
           dnl Fallback to older version
           [PKG_CHECK_MODULES([ZRTPCPP], libzrtpcpp >= 1.3.0, [with_zrtp_configure=no],
               AC_MSG_ERROR([Missing zrtp development files]))])
    ]);

dnl This is a workaround until libzrtpcpp gets a version macro
AC_DEFINE_UNQUOTED([HAVE_ZRTP], `if test "x$with_zrtp" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have zrtp support])
AC_DEFINE_UNQUOTED([HAVE_ZRTP_CONFIGURE], `if test "x$with_zrtp_configure" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have zrtp >= 2.3.0])
AM_CONDITIONAL(BUILD_ZRTP, test "x$with_zrtp" = "xyes" )

# Instant Messaging
# required dependency(ies): libxpat
AC_ARG_WITH([instant_messaging],
    [AS_HELP_STRING([--without-instant_messaging], [disable support for instant-messaging])],
    [],
    [with_instant_messaging=yes])
    AS_IF([test "x$with_instant_messaging" = "xyes"], [
        PKG_CHECK_MODULES([EXPAT], expat >= 2.0.0, [with_instant_messaging=yes],
           dnl Fallback to older version
           [with_instant_messaging=no])
    ]);

AC_DEFINE_UNQUOTED([HAVE_INSTANT_MESSAGING], `if test "x$with_instant_messaging" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have instant messaging support])
AM_CONDITIONAL(BUILD_INSTANT_MESSAGING, test "x$with_instant_messaging" = "xyes" )


# PTHREAD
# required dependency(ies): libxpat
AX_PTHREAD


# SDES Key Exchange
# required dependency(ies): libpcre
AC_ARG_WITH([sdes],
    [AS_HELP_STRING([--without-sdes], [disable support for sdes key exchange])],
    [],
    [with_sdes=yes])
AS_IF([test "x$with_sdes" = "xyes"], [
      PKG_CHECK_MODULES(PCRE, [libpcre],,AC_MSG_ERROR([libpcre not found]))]);

AC_DEFINE_UNQUOTED([HAVE_SDES], `if test "x$with_sdes" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have sdes support])
AM_CONDITIONAL(BUILD_SDES, test "x$with_sdes" = "xyes" )


dnl Check for libcppunit-dev
CPPUNIT_MIN_VERSION=1.12
PKG_CHECK_MODULES(CPPUNIT, cppunit >= ${CPPUNIT_MIN_VERSION}, AM_CONDITIONAL(BUILD_TEST, test 1 = 1 ), AM_CONDITIONAL(BUILD_TEST, test 0 = 1 ))


# GSM CODEC
# required dependency(ies): libgsm
dnl check for libgsm1 (doesn't use pkg-config)
dnl Check for libgsm
AC_ARG_WITH([gsm], [AS_HELP_STRING([--without-gsm],
            [disable support for gsm codec])], [], [with_gsm=yes])

LIBGSM=
AS_IF([test "x$with_gsm" != xno],
        [AC_CHECK_HEADER([gsm/gsm.h], , AC_MSG_FAILURE([Unable to find the libgsm1 headers (you may need to install the dev package).  You may use --without-gsm to compile without gsm codec support.]))]
        [AC_CHECK_LIB([gsm], [gsm_decode], [], [
            AC_MSG_FAILURE([libgsm link test failed. You may use --without-gsm to compile without gsm codec support.])
        ])
    ])

AC_DEFINE_UNQUOTED([HAVE_GSM], `if test "x$with_gsm" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have libgsm])
AM_CONDITIONAL(BUILD_GSM, test "x$with_gsm" = "xyes" )


# SPEEX CODEC
# required dependency(ies): libspeex
dnl Check for libspeex
AC_ARG_WITH([speex],
        [AS_HELP_STRING([--without-speex], [disable support for speex codec])],
        [],
        [with_speex=yes])

AS_IF([test "x$with_speex" != xno], [PKG_CHECK_MODULES([SPEEX], [speex])])

AC_DEFINE_UNQUOTED([HAVE_SPEEX], `if test "x$with_speex" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have libspeex])
AM_CONDITIONAL(BUILD_SPEEX, test "x$with_speex" = "xyes" )


# SPEEX DSP
# required dependency(ies): libspeexdsp
dnl check in case the libspeexdsp is not installed
AC_ARG_WITH([speexdsp],
    [AS_HELP_STRING([--without-speexdsp],
    [disable support for speexdp Noise Suppression and Automatic Gain Control])],
    [],
    [with_speexdsp=yes])

AS_IF([test "x$with_speexdsp" != xno], [PKG_CHECK_MODULES([SPEEXDSP], [speexdsp])])

AC_DEFINE_UNQUOTED([HAVE_SPEEXDSP], `if test "x$with_speexdsp" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have libspeexdsp])
AM_CONDITIONAL(BUILD_SPEEXDSP, test "x$with_speexdsp" = "xyes" )

dnl iLBC is enabled if it's installed
AC_ARG_WITH([libilbc],
  [AS_HELP_STRING([--with-libilbc],
    [support ilbc audio @<:@default=check@:>@])],
  [],
  [with_libilbc=check])
AS_CASE(["$with_libilbc"],
  [yes], [PKG_CHECK_MODULES([libilbc], [libilbc], [HAVE_LIBILBC=1])],
  [no], [HAVE_LIBILBC=0],
  [PKG_CHECK_MODULES([libilbc], [libilbc], [HAVE_LIBILBC=1], [HAVE_LIBILBC=0])])
AM_CONDITIONAL([BUILD_ILBC], [test "$HAVE_LIBILBC" -eq 1])

dnl opus is enabled if it's installed
AC_ARG_WITH([opus],
  [AS_HELP_STRING([--with-opus], [support opus audio @<:@default=check@:>@])], [], [with_opus=check])
AS_CASE(["$with_opus"],
  [yes], [PKG_CHECK_MODULES([opus], [opus], [HAVE_OPUS=1])],
  [no], [HAVE_OPUS=0],
  [PKG_CHECK_MODULES([opus], [opus], [HAVE_OPUS=1], [HAVE_OPUS=0])])
AM_CONDITIONAL([BUILD_OPUS], [test "$HAVE_OPUS" -eq 1])

AC_ARG_WITH([iax],
    AS_HELP_STRING([--without-iax], [Ignore presence of iax and disable it]))

AS_IF([test "x$with_iax" != "xno"],
      [AC_CHECK_HEADER("iax/iax-client.h", [have_iax=yes], [have_iax=no])],
      [have_iax=no])

# only fail if IAX was explicitly requested but not found
AS_IF([test "x$have_iax" = "xyes"],
      [AC_DEFINE([HAVE_IAX], 1, [Define if you have libiax])
       AM_CONDITIONAL(USE_IAX, `true`)],
      [AS_IF([test "x$with_iax" = "xyes"],
             [AC_MSG_ERROR([iax requested but not found])])
       AC_DEFINE([HAVE_IAX], 0, [Define if you have libiax])
       AM_CONDITIONAL(USE_IAX, `false`)])

# dht is default-enabled, but requires gnutls
AC_ARG_ENABLE([dht],
  AS_HELP_STRING([--disable-dht], [disable support for dht]))

AS_IF([test "x$enable_dht" != "xno" -a "$HAVE_GNUTLS" -eq 1],
      [AC_DEFINE([HAVE_DHT], 1, [Define to enable dht])
       AM_CONDITIONAL(USE_DHT, true)],
      [AC_DEFINE([HAVE_DHT], 0, [Define to enable dht])
       AM_CONDITIONAL(USE_DHT, false)])

# NETWORK MANAGER
# required dependency(ies): libiax2 (static)
dnl Check for network-manager
AC_ARG_WITH([networkmanager], [AS_HELP_STRING([--without-networkmanager],
            [disable support for network-manager events])], [],
            [with_networkmanager=yes])

AM_CONDITIONAL(USE_NETWORKMANAGER, test "x$with_networkmanager" = "xyes" )

dnl IPv6 mode is default-disabled
AC_ARG_ENABLE([ipv6], AS_HELP_STRING([--enable-ipv6], [Enable IPv6 support]))

AC_DEFINE_UNQUOTED([HAVE_IPV6], `if test "x$enable_ipv6" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have IPv6])
AM_CONDITIONAL(BUILD_IPV6, test "x$enable_ipv6" = "xyes" )


# DOXYGEN
# required dependency(ies): doxygen
# check for doxygen, mostly stolen from http://log4cpp.sourceforge.net/
# ----------------------------------------------------------------------------
AC_DEFUN([BB_ENABLE_DOXYGEN],
    [
    AC_ARG_ENABLE(doxygen, [  --enable-doxygen        enable documentation generation with doxygen (disabled by default)])
    AC_ARG_ENABLE(dot, [  --enable-dot            use 'dot' to generate graphs in doxygen (auto)])
    if test "x$enable_doxygen" = xyes; then
        AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
        test x$DOXYGEN = x && AC_MSG_ERROR([could not find doxygen])
        AC_PATH_PROG(DOT, dot, , $PATH)
        test x$DOT = x -a "x$enable_dot" = xyes && AC_MSG_ERROR([could not find dot])
    fi
    AM_CONDITIONAL(ENABLE_DOXYGEN, test x$DOXYGEN = xyes)
])

# Actually perform the doxygen check
BB_ENABLE_DOXYGEN


dnl debug mode is default-disabled
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [Build in debug mode, adds stricter warnings, disables optimization]))

AS_IF([test "x$enable_debug" = "xyes"],
      [CXXFLAGS="${CXXFLAGS} -g -Wno-return-type -Wall -Wextra -Wnon-virtual-dtor -O0"])

dnl What Makefiles to generate

#TODO: split this list across where the relevant check is performed
AC_CONFIG_FILES([Makefile \
                 bin/Makefile \
                 src/Makefile \
                 src/sip/Makefile \
                 src/im/Makefile \
                 src/iax/Makefile \
                 src/dht/Makefile \
                 src/dht/dhtcpp/Makefile \
                 src/audio/Makefile \
                 src/audio/audiortp/Makefile \
                 src/audio/pulseaudio/Makefile \
                 src/audio/alsa/Makefile \
                 src/audio/opensl/Makefile \
                 src/audio/jack/Makefile \
                 src/audio/coreaudio/Makefile \
                 src/audio/sound/Makefile \
                 src/audio/codecs/Makefile \
                 src/config/Makefile \
                 src/client/Makefile \
                 src/hooks/Makefile \
                 src/history/Makefile \
                 src/video/Makefile \
                 src/video/v4l2/Makefile \
                 src/video/test/Makefile \
                 test/Makefile \
                 tools/Makefile \
                 ringtones/Makefile \
                 man/Makefile \
                 doc/Makefile \
                 doc/doxygen/Makefile])

# Go!
AC_OUTPUT
