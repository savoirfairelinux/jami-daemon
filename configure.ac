dnl Jami - configure.ac

dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([Jami Daemon],[13.7.0],[jami@gnu.org],[jami])

dnl Clear the implicit flags that default to '-g -O2', otherwise they
dnl take precedence over the values we set via the
dnl DAEMONCFLAGS/DAEMONCXXFLAGS variables below.
: ${CFLAGS=""}
: ${CXXFLAGS=""}

AC_COPYRIGHT([[Copyright (c) Savoir-faire Linux Inc. 2004-2023]])
AC_REVISION([$Revision$])

dnl Where to find configure files
AC_CONFIG_SRCDIR(src/ring_api.cpp)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([-Wno-portability subdir-objects])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR]) # required by automake 1.12
AC_CONFIG_HEADERS([config.h])

# Silent build by default. Use make V=1 to increase verbosity
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl debug mode is default-disabled
AC_ARG_ENABLE([debug],
  AS_HELP_STRING([--enable-debug],
    [Build in debug mode, adds stricter warnings, disables optimization]))

AS_IF([test "x$enable_debug" = "xyes"],
  [DAEMONCFLAGS+=" -g -fno-omit-frame-pointer -Wall -Wextra -Wnon-virtual-dtor -Wno-deprecated -Wno-deprecated-declarations \
-Wno-unknown-pragmas -Wformat=2 -O0"
   DAEMONCXXFLAGS+=" -g -fno-omit-frame-pointer -Wall -Wextra -Wnon-virtual-dtor -Wno-deprecated -Wno-deprecated-declarations \
-Wno-unknown-pragmas -Wformat=2 -O0"],
  [DAEMONCFLAGS+=" -DNDEBUG=1 -O3"
   DAEMONCXXFLAGS+=" -DNDEBUG=1 -O3 -Wno-deprecated"])

AC_ARG_ENABLE([fuzzing],
  AS_HELP_STRING([--enable-fuzzing],
    [Build fuzzing tools]))
AM_CONDITIONAL([ENABLE_FUZZING], [test "x$enable_fuzzing" = "xyes"])
AM_COND_IF([ENABLE_FUZZING], [AC_CONFIG_FILES([test/fuzzing/Makefile])])

AC_ARG_ENABLE([agent],
  AS_HELP_STRING([--enable-agent],
    [Build agent]))
AM_CONDITIONAL([ENABLE_AGENT], [test "x$enable_agent" = "xyes"])
AM_COND_IF([ENABLE_AGENT],
           [AC_CONFIG_FILES([test/agent/Makefile
                             test/agent/pre-inst-env:test/agent/build-aux/pre-inst-env.in
                             test/agent/scenarios/bulk-calls/run-scenario],
                            [chmod +x test/agent/pre-inst-env
                             chmod +x test/agent/scenarios/bulk-calls/run-scenario])])

AC_ARG_ENABLE([tracepoints], AS_HELP_STRING([--enable-tracepoints], [Enable tracepoints]))

AS_IF([test "x$enable_tracepoints" = "xyes"],
  [AC_DEFINE(ENABLE_TRACEPOINTS, [], [Static tracepoints enabled])
   AM_CONDITIONAL(ENABLE_TRACEPOINTS, true)],
  [AM_CONDITIONAL(ENABLE_TRACEPOINTS, false)])

dnl Check for programs
AC_PROG_CC
AC_PROG_CXX
AC_PROG_OBJCXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_LANG(C++)

dnl
dnl  Check the operating system
dnl
HAVE_WIN64="0"
HAVE_IOS="0"
HAVE_OSX="0"

case "${host_os}" in
  "")
    SYS=unknown
    ;;
  linux*)
    SYS=linux
    # Necessary for linking .a to a .so
    DAEMONLDFLAGS+=" -Wl,-Bsymbolic"
    # Needed for plugin system
    DAEMONLDFLAGS+=" -ldl"
    ;;
  darwin*)
    SYS=darwin
    DAEMONCFLAGS+=" -arch $ARCH -isysroot $SDKROOT"
    DAEMONCXXFLAGS+=" -arch $ARCH -isysroot $SDKROOT"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
        [[#import <TargetConditionals.h>
          #if TARGET_OS_IPHONE == 0
          #error this is not an IPHONE
          #endif
        ]])],
        [HAVE_IOS="1"],
        [HAVE_OSX="1"])
    ;;
  mingw32*)
    SYS=mingw32
    AC_CHECK_TOOL(WINDRES, windres, :)
    AC_CHECK_TOOL(OBJCOPY, objcopy, :)
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

    AC_DEFINE([WIN32_LEAN_AND_MEAN],[1], [Define to limit the scope of <windows.h>.])

    DAEMONCPPFLAGS+=" -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -D__USE_MINGW_ANSI_STDIO=1"
    DAEMONLDFLAGS+=" -Wl,--nxcompat -Wl,--dynamicbase"
    LIBS+=" -lws2_32 -lwsock32 -lshlwapi"
    ac_default_prefix="`pwd`/_win32"
    DESTDIR="`pwd`/_win32/"
    AC_SUBST(WINDOWS_ARCH)
    AC_SUBST(PROGRAMFILES)

    case "${host}" in
      amd64*|x86_64*)
        HAVE_WIN64="1"
        ;;
    esac
    ;;
  *)
    SYS="${host_os}"
    ;;
esac
AM_CONDITIONAL(HAVE_DARWIN,  test "${SYS}" = "darwin")
AM_CONDITIONAL(HAVE_LINUX,   test "${SYS}" = "linux")
AM_CONDITIONAL(HAVE_WIN32,   test "${SYS}" = "mingw32")
AM_CONDITIONAL(HAVE_WIN64,   test "${HAVE_WIN64}" = "1")
AM_CONDITIONAL(HAVE_OSX,     test "${HAVE_OSX}" = "1")
AM_CONDITIONAL(HAVE_IOS,     test "${HAVE_IOS}" = "1")

AC_DEFINE([MSGPACK_DISABLE_LEGACY_NIL], [1], [msgpack])

dnl FIXME this should be deduced automatically
AC_DEFINE_UNQUOTED([HAVE_COREAUDIO],
  `if test "${HAVE_OSX}" = "1" || test "${HAVE_IOS}" = "1"; \
then echo 1; else echo 0; fi`,
   [Define if you have CoreAudio])

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

AS_IF([test "$SYS" = linux && test "${HAVE_ANDROID}" != "1"],
  [DAEMONCPPFLAGS+=" -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"]
  [])

dnl override platform specific check for dependent libraries
dnl otherwise libtool linking of shared libraries will
dnl fail on anything other than pass_all.
AC_CACHE_VAL(lt_cv_deplibs_check_method,
    [lt_cv_deplibs_check_method=pass_all])

LT_INIT([dlopen win32-dll shared static])

dnl Define C++ as default language
LT_LANG(C++)

lt_cv_deplibs_check_method=pass_all

dnl Check for C++17 support
AX_CXX_COMPILE_STDCXX(17,[ext],[mandatory])

dnl Check for header files
AC_CHECK_HEADERS([arpa/inet.h cxxabi.h fcntl.h libintl.h limits.h \
                  netdb.h netinet/in.h stdlib.h string.h \
                  sys/ioctl.h sys/socket.h sys/time.h unistd.h])

dnl Check for typedefs, structures, and compiler characteristics
AC_TYPE_PID_T

PKG_PROG_PKG_CONFIG()

dnl On some OS we need static linking
AS_IF([test -n "${PKG_CONFIG}"],
  [AS_IF([test "${SYS}" = "mingw32" -o "${SYS}" = "darwin"],
     [PKG_CONFIG="${PKG_CONFIG} --static"])])

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

dnl define LIBJAMI_BUILD because we are building libjami, not using it
dnl if building shared library, define jami_EXPORTS
AC_MSG_CHECKING([if compiling shared library])
DAEMONCPPFLAGS+=" -fvisibility=hidden -DLIBJAMI_BUILD"
AS_IF([test "x$enable_shared" == "xyes"],
  [RING_SHARED=yes
   DAEMONCPPFLAGS+=" -Djami_EXPORTS"],
  [RING_SHARED=no])
AC_MSG_RESULT([$RING_SHARED])

DAEMONCPPFLAGS+=" -DASIO_STANDALONE"

dnl
dnl  Check for the contrib directory
dnl
AC_ARG_WITH(contrib,
  [AS_HELP_STRING([--with-contrib[=DIR]],
    [search for 3rd party libraries in DIR/include and DIR/lib])])
AC_MSG_CHECKING([for 3rd party libraries path])
AS_IF([test -z "${with_contrib}" || test "${with_contrib}" = "yes"],
  [CONTRIB_DIR="${srcdir}/contrib/${host}"
   AS_IF([test ! -d "${CONTRIB_DIR}"],
     [echo "${CONTRIB_DIR} not found" >&AS_MESSAGE_LOG_FD
      CONTRIB_DIR="${srcdir}/contrib/`$CC -dumpmachine`"
      AS_IF([test ! -d "${CONTRIB_DIR}"],
        [echo "${CONTRIB_DIR} not found" >&AS_MESSAGE_LOG_FD
         CONTRIB_DIR=""
         AC_MSG_RESULT([not found])])])],
  [AS_IF([test "${with_contrib}" != "no"],
     [CONTRIB_DIR="${with_contrib}"],
     [CONTRIB_DIR=""
      AC_MSG_RESULT([disabled])])])

AS_IF([test -n "${CONTRIB_DIR}"],
  [AS_IF([test -d "${CONTRIB_DIR}/lib"],
    [CONTRIB_DIR=`cd "${CONTRIB_DIR}" && pwd`],
    [echo "${CONTRIB_DIR}/lib not found" >&AS_MESSAGE_LOG_FD
     CONTRIB_DIR=""
     AC_MSG_RESULT([not usable])])])

AS_IF([test -n "${CONTRIB_DIR}"],
  [AC_MSG_RESULT([${CONTRIB_DIR}])
   export PATH=${CONTRIB_DIR}/bin:$PATH

   DAEMONCPPFLAGS+=" -I${CONTRIB_DIR}/include  -DPJ_AUTOCONF=1"

   AS_IF([test "${HAVE_WIN64}" = "1"],
     [DAEMONCPPFLAGS+=" -DPJ_WIN64=1"])

   AS_IF([test "${SYS}" = "mingw32"],
     [PKG_CONFIG_PATH_CUSTOM="${CONTRIB_DIR}/lib/pkgconfig:\
${CONTRIB_DIR}/lib64/pkgconfig"
      AC_SUBST(PKG_CONFIG_PATH_CUSTOM)
      export PKG_CONFIG_PATH_CUSTOM])

   export PKG_CONFIG_PATH="${CONTRIB_DIR}/lib/pkgconfig:\
${CONTRIB_DIR}/lib64/pkgconfig:$PKG_CONFIG_PATH"
   DAEMONLDFLAGS+=" -L${CONTRIB_DIR}/lib"

   AS_IF([test "${SYS}" = "darwin"],
     [export LD_LIBRARY_PATH="${CONTRIB_DIR}/lib:$LD_LIBRARY_PATH"
      export DYLD_LIBRARY_PATH="${CONTRIB_DIR}/lib:$DYLD_LIBRARY_PATH"])],
  [AS_IF([test -n "${with_contrib}" && test "${with_contrib}" != "no"],
     [AC_MSG_ERROR([Third party libraries not found!])])])
AC_SUBST(CONTRIB_DIR)

dnl Add extras/tools to the PATH
TOOLS_DIR="${srcdir}/extras/tools/build/bin"
AS_IF([test -d "${TOOLS_DIR}"],
  [TOOLS_DIR=`cd "${TOOLS_DIR}" && pwd`
   export PATH="${TOOLS_DIR}":$PATH])

dnl Check for zlib
PKG_CHECK_MODULES(ZLIB, zlib,, AC_MSG_ERROR([zlib not found]))

PKG_CHECK_MODULES(LIBGIT2, [libgit2 >= 1.1.0],, AC_MSG_ERROR([Missing libgit2 files]))

dnl Check for pjproject
PKG_CHECK_MODULES(PJPROJECT, libpjproject,,
  AC_MSG_ERROR([Missing pjproject files]))

dnl Guile
AM_COND_IF([ENABLE_AGENT],
  [PKG_CHECK_MODULES(GUILE, [guile-3.0 >= 3.0.7],, AC_MSG_ERROR([Missing guile files]))],)

PKG_CHECK_MODULES([YAMLCPP], [yaml-cpp >= 0.5.1],,
  AC_MSG_ERROR([yaml-cpp not found]))

PKG_CHECK_MODULES([YAMLCPP], [yaml-cpp >= 0.5.1],,
  AC_MSG_ERROR([yaml-cpp not found]))

PKG_CHECK_MODULES([JSONCPP], [jsoncpp >= 1.6.5],
  [DAEMONCPPFLAGS+=" ${JSONCPP_CFLAGS}"
   DAEMONLDFLAGS+=" ${JSONCPP_LIBS}"],
  AC_MSG_ERROR([jsoncpp not found]))

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

if test "${SYS}" = "linux"; then
  dnl Check for alsa development package - name: libasound2-dev
  ALSA_MIN_VERSION=1.0
  AC_ARG_WITH([alsa],
    [AS_HELP_STRING([--without-alsa], [disable support for alsa])],
    [],
    [with_alsa=yes])
  AS_IF([test "x$with_alsa" = "xyes"],
    [PKG_CHECK_MODULES(ALSA, alsa >= ${ALSA_MIN_VERSION},,
       AC_MSG_ERROR([Missing alsa development files]))]);

  AC_DEFINE_UNQUOTED([HAVE_ALSA],
    `if test "x$with_alsa" = "xyes"; then echo 1; else echo 0; fi`,
    [Define if you have alsa])
fi
AM_CONDITIONAL(BUILD_ALSA, test "x$with_alsa" = "xyes")


if test "${SYS}" = "linux"; then
  dnl Check for pulseaudio development package - name: libpulse-dev
  LIBPULSE_MIN_VERSION=0.9.15
  AC_ARG_WITH([pulse],
    [AS_HELP_STRING([--without-pulse], [disable support for pulseaudio])],
    [],
    [with_pulse=yes])

  AS_IF([test "x$with_pulse" = "xyes"],
    [PKG_CHECK_MODULES(PULSEAUDIO, libpulse >= ${LIBPULSE_MIN_VERSION},,
     AC_MSG_ERROR([Missing pulseaudio development files]))]);

  AC_DEFINE_UNQUOTED([HAVE_PULSE],
    `if test "x$with_pulse" = "xyes"; then echo 1; else echo 0; fi`,
    [Define if you have pulseaudio])
fi
AM_CONDITIONAL(BUILD_PULSE, test "x$with_pulse" = "xyes")

AC_ARG_WITH([jack],
  AS_HELP_STRING([--without-jack], [Ignore presence of jack and disable it]))

AS_IF([test "x$with_jack" != "xno"],
  [PKG_CHECK_MODULES(JACK, jack, [have_jack=yes], [have_jack=no])],
  [have_jack=no])

AS_IF([test "x$have_jack" = "xyes"],,
  [AS_IF([test "x$with_jack" = "xyes"],
     [AC_MSG_ERROR([jack requested but not found])])])

dnl check for libportaudio
AS_IF([test "${SYS}" = "mingw32"],[
AC_ARG_WITH([portaudio], [AS_HELP_STRING([--without-portaudio],
            [disable support for portaudio])], [], [with_portaudio=yes])])
AS_IF([test "x$with_portaudio" = "xyes"],
  [PKG_CHECK_MODULES(PORTAUDIO, [portaudio-2.0],
    [AC_DEFINE([HAVE_PORTAUDIO], 1, [Define if you have portaudio])],
    [AC_MSG_ERROR([Missing libportaudio development files])])])

AM_CONDITIONAL(BUILD_PORTAUDIO, test "x$with_portaudio" = "xyes")
AM_CONDITIONAL(HAVE_PORTAUDIO, test "x$with_portaudio" = "xyes")

AC_DEFINE_UNQUOTED([HAVE_JACK],
  `if test "x$have_jack" = "xyes"; then echo 1; else echo 0; fi`,
  [Define if you have jack])
AM_CONDITIONAL(BUILD_JACK, test "x$have_jack" = "xyes")

dnl Coverage is default-disabled
AC_ARG_ENABLE([coverage], AS_HELP_STRING([--enable-coverage], [Enable coverage]))

AS_IF([test "x$enable_coverage" = "xyes"],
  [DAEMONCXXFLAGS+=" --coverage"
   DAEMONLDFLAGS+=" --coverage"])

# DBUSCPP
dnl Check for dbuscpp, the C++ bindings for D-Bus
AC_ARG_WITH([dbus],
  [AS_HELP_STRING([--without-dbus], [disable support for dbus])],
  [],
  [with_dbus=yes])
AS_IF([test "x$with_dbus" = "xyes"],
  [PKG_CHECK_MODULES(DBUSCPP, dbus-c++-1,,
     AC_MSG_WARN([Missing dbus development files]))

   AC_CONFIG_FILES([bin/dbus/Makefile])

   AC_PATH_PROG([DBUSXX_XML2CPP], dbusxx-xml2cpp,
     [AC_MSG_ERROR([dbusxx-xml2cpp not found])])

   AS_IF([test "x$CLANG" = "xyes"],
     [AC_MSG_ERROR([dbus does not compile when clang is used,
                    rerun with --without-dbus])]);
   AM_CONDITIONAL(RING_DBUS, true)],
  [TESTS= $(check_PROGRAMS)
   AM_CONDITIONAL(RING_DBUS, false)]);

dnl name service is default-enabled
AC_ARG_ENABLE([ringns], AS_HELP_STRING([--disable-ringns],
  [Enable Name Service]))
AM_CONDITIONAL([RINGNS], test "x$enable_ringns" != "xno")
AC_DEFINE_UNQUOTED([HAVE_RINGNS],
  `if test "x$enable_ringns" != "xno"; then echo 1; else echo 0; fi`,
  [Define if you use the Name Service])
AM_COND_IF([RINGNS],
  [PKG_CHECK_MODULES(LIBCRYPTO, libcrypto,,
     AC_MSG_ERROR([Missing libcrypto development files]))
   PKG_CHECK_MODULES(LIBSSL, libssl,,
     AC_MSG_ERROR([Missing libssl development files]))],
  []);

dnl nodejs module
AC_ARG_WITH([nodejs], AS_HELP_STRING([--with-nodejs], [Enable NodeJS module]))
AM_CONDITIONAL(ENABLE_NODEJS, [test "x$with_nodejs" = "xyes"])
AM_COND_IF([ENABLE_NODEJS],
  [AC_PATH_PROG(SWIG, swig, "")
   AS_AC_EXPAND(SBINDIR, $sbindir)
   AC_SUBST(SBINDIR)
   AC_CONFIG_FILES([bin/nodejs/Makefile])])

dnl Check for libav
PKG_CHECK_MODULES(LIBAVUTIL, libavutil >= 54.31.100,,
  AC_MSG_ERROR([Missing libavutil development files]))

PKG_CHECK_MODULES(LIBAVCODEC, libavcodec >= 56.60.100,,
  AC_MSG_ERROR([Missing libavcodec development files]))
LIBAVCODEC_CFLAGS="${LIBAVCODEC_CFLAGS} -D__STDC_CONSTANT_MACROS"

PKG_CHECK_MODULES(LIBAVFORMAT, libavformat >= 56.40.101,,
  AC_MSG_ERROR([Missing libavformat development files]))

PKG_CHECK_MODULES(LIBAVDEVICE, libavdevice >= 56.4.100,,
  AC_MSG_ERROR([Missing libavdevice development files]))

PKG_CHECK_MODULES(LIBAVFILTER, libavfilter >= 5.40.101,,
  AC_MSG_ERROR([Missing libavfilter development files]))

PKG_CHECK_MODULES(LIBSWSCALE, libswscale >= 3.1.101,,
  AC_MSG_ERROR([Missing libswscale development files]))

PKG_CHECK_MODULES(LIBSWRESAMPLE, libswresample >= 1.2.101,,
  AC_MSG_ERROR([Missing libswresample development files]))

dnl Jami Plugin is default-enabled
AC_ARG_ENABLE([plugin], AS_HELP_STRING([--disable-plugin], [Disable plugin]))
AS_IF([test "x$enable_plugin" != "xno"],
  [AC_DEFINE(ENABLE_PLUGIN, [], [Jami Plugin support enabled])
   AM_CONDITIONAL(ENABLE_PLUGIN, true)
   AS_IF([test "$SYS" = linux],
     [PKG_CHECK_MODULES([ARCHIVE], [libarchive >= 3.0],,
      AC_MSG_ERROR([libarchive not found]))],
     [AS_IF([test "$SYS" = darwin],
     [PKG_CHECK_MODULES([MINIZIP], [minizip >= 3.0.0],,
      AC_MSG_ERROR([libminizip not found]))],
     [])]
     )],
  [AM_CONDITIONAL(ENABLE_PLUGIN, false)]);

dnl Video is default-enabled
AC_ARG_ENABLE([video], AS_HELP_STRING([--disable-video], [Disable video]))

AS_IF([test "x$enable_video" != "xno"],
  [AC_DEFINE(ENABLE_VIDEO, [], [Video support enabled])
   AM_CONDITIONAL(ENABLE_VIDEO, true)
   AS_IF([test "$SYS" = linux && test -z "${HAVE_ANDROID_FALSE}"],
     [PKG_CHECK_MODULES(UDEV, libudev,,
        AC_MSG_ERROR([Missing libudev development files]))],
     [])],
  [AM_CONDITIONAL(ENABLE_VIDEO, false)]);

AC_ARG_ENABLE([accel],
  AS_HELP_STRING([--disable-accel],
    [Disable all hardware accelerations (enabled by default)]))
AS_IF([test "x$enable_video" != "xno" -a "x$enable_accel" != "xno"],
  dnl if new hwaccel api is found, activate hardware accel
  dnl prefix needs to be different from the one above
  [PKG_CHECK_MODULES(AVUTIL, libavutil >= 55.75.100,
     [ring_accel="yes"
      AC_DEFINE([RING_ACCEL], [1], [Hardware acceleration is enabled])
      AS_IF([test "${SYS}" = "darwin"],
        [AC_CHECK_HEADER([VideoToolbox/VideoToolbox.h],
           [AC_CHECK_HEADER([libavcodec/videotoolbox.h],
              [ring_vt="yes"
               AC_DEFINE([ENABLE_VIDEOTOOLBOX], [1],
                 [Defined if videotoolbox is available])])])])],
     [])])
AM_CONDITIONAL([RING_ACCEL], [test "x${ring_accel}" = "xyes"])

dnl check for GnuTLS
PKG_CHECK_MODULES([GNUTLS], [gnutls >= 3.6.7], [], AC_MSG_ERROR([gnutls not found]))
PKG_CHECK_MODULES([NETTLE], [nettle >= 3.0.0], [], AC_MSG_ERROR([nettle not found]))

# PTHREAD
# required dependency: libxpat
AX_PTHREAD

dnl Check for libcppunit-dev
CPPUNIT_MIN_VERSION=1.12
PKG_CHECK_MODULES(CPPUNIT, cppunit >= ${CPPUNIT_MIN_VERSION},
  AM_CONDITIONAL(BUILD_TEST, test 1 = 1 ),
  AM_CONDITIONAL(BUILD_TEST, test 0 = 1 ))


# SPEEX CODEC
# required dependency: libspeex
dnl Check for libspeex
AC_ARG_WITH([speex],
  [AS_HELP_STRING([--without-speex], [disable support for speex codec])],
  [],
  [with_speex=yes])

AS_IF([test "x$with_speex" != xno], [PKG_CHECK_MODULES([SPEEX], [speex])])

AC_DEFINE_UNQUOTED([HAVE_SPEEX],
  `if test "x$with_speex" = "xyes"; then echo 1; else echo 0; fi`,
  [Define if you have libspeex])
AM_CONDITIONAL(BUILD_SPEEX, test "x$with_speex" = "xyes" )

# SPEEX DSP
# required dependency: libspeexdsp
dnl check in case the libspeexdsp is not installed
AC_ARG_WITH([speexdsp],
  [AS_HELP_STRING([--without-speexdsp],
  [disable support for speexdp Noise Suppression and Automatic Gain Control])],
  [],
  [with_speexdsp=yes])

AS_IF([test "x$with_speexdsp" != xno],
  [PKG_CHECK_MODULES([SPEEXDSP], [speexdsp])])
AC_DEFINE_UNQUOTED([HAVE_SPEEXDSP],
  `if test "x$with_speexdsp" = "xyes"; then echo 1; else echo 0; fi`,
  [Define if you have libspeexdsp])
AM_CONDITIONAL(BUILD_SPEEXDSP, test "x$with_speexdsp" = "xyes" )

# WEBRTC AUDIO PROCESSING
if test "${SYS}" = "linux"; then
  dnl Check for webrtc-audio-processing development package
  dnl - name: libwebrtc-audio-processing-dev
  AC_ARG_WITH([webrtcap],
    [AS_HELP_STRING([--without-webrtcap],
       [disable support for webrtc-audio-processing])],
    [], [with_webrtc_ap=yes])

  AS_IF([test "x$with_webrtc_ap" = "xyes"],
    [PKG_CHECK_MODULES(WEBRTC, [webrtc-audio-processing >= 0.3],
      [AC_DEFINE([HAVE_WEBRTC_AP], 1,
         [Define if you have webrtc-audio-processing])],
      [AC_MSG_WARN([Missing webrtc-audio-processing development files])])])
fi
AM_CONDITIONAL([HAVE_WEBRTC_AP], test "x$with_webrtc_ap" = "xyes")

dnl check for openDHT
PKG_CHECK_MODULES([OPENDHT], [opendht >= 2.3.2],,
  AC_MSG_FAILURE([Missing OpenDHT]))

PKG_CHECK_MODULES([SECP256K1], [libsecp256k1 >= 0.1],,
  AC_MSG_FAILURE([Missing secp256k1]))

PKG_CHECK_MODULES([FMT], [fmt >= 7.0],,
  AC_MSG_FAILURE([Missing {fmt}]))

# LIBUPNP
dnl check for libupnp
AC_ARG_WITH([upnp], [AS_HELP_STRING([--without-upnp],
  [disable support for upnp])], [], [with_upnp=yes])

AS_IF([test "x$with_upnp" = "xyes"],
  [PKG_CHECK_MODULES(LIBUPNP, [libupnp],
    [AC_DEFINE([HAVE_LIBUPNP], 1, [Define if you have libupnp])],
    [AC_MSG_WARN([Missing libupnp development files])
     AC_DEFINE([HAVE_LIBUPNP], 0, [Define if you have libupnp])])])
AM_CONDITIONAL([BUILD_PUPNP], test "x$with_upnp" = "xyes")

# LIBNATPMP
dnl check for libnatpmp
AC_ARG_WITH([natpmp], [AS_HELP_STRING([--without-natpmp],
  [disable support for NAT-PMP])], [], [with_natpmp=yes])

AS_IF([test "x$with_natpmp" != xno],
  [AC_CHECK_HEADER([natpmp.h],
     AC_CHECK_LIB([natpmp], [initnatpmp], [], [with_natpmp=no]),
       [AC_MSG_WARN([Unable to find the libnatpmp headers
          (you may need to install the dev package). You may use
          --without-natpmp to compile without NAT-PMP support.]);
        with_natpmp=no])],
  [])

AC_SUBST(COREAUDIO_CFLAGS)
AC_SUBST(COREAUDIO_LIBS)
AC_CHECK_HEADERS([CoreAudio/CoreAudio.h, AudioUnit/AudioUnit.h CoreServices/CoreServices.h AudioToolbox/AudioToolbox.h])
AC_CHECK_LIB([coreaudio])
COREAUDIO_LIBS="-framework CoreAudio -framework Carbon -framework AudioUnit"
DAEMONCPPFLAGS+=" ${COREAUDIO_LIBS}"
DAEMONLDFLAGS+=" ${COREAUDIO_LIBS}"

AC_DEFINE_UNQUOTED([HAVE_LIBNATPMP],
  `if test "x$with_natpmp" != xno; then echo 1; else echo 0; fi`,
  [Define if you have libnatpmp])
AM_CONDITIONAL([BUILD_NATPMP], test "x$with_natpmp" = "xyes")

# SHM
dnl check for shm
AS_IF([test "x$with_dbus" == xyes],
  AC_DEFINE([ENABLE_SHM], [1], [Define if you have shared memory support]))

# LTTNG
AM_COND_IF([ENABLE_TRACEPOINTS],
 [PKG_CHECK_MODULES(LTTNG, [lttng-ust >= 2.13],, AC_MSG_ERROR([Missing lttng-ust]))
  PKG_CHECK_MODULES(LIBURCU, [liburcu >= 0.13.1],, AC_MSG_ERROR([Missng liburcu]))])

# DOXYGEN
# required dependency(ies): doxygen
# check for doxygen, mostly stolen from http://log4cpp.sourceforge.net/
# ----------------------------------------------------------------------------
AC_DEFUN([BB_ENABLE_DOXYGEN],
  [AC_ARG_ENABLE(doxygen,
     [AS_HELP_STRING([--enable-doxygen],
        [enable documentation generation with doxygen (disabled by default)])])
   AC_ARG_ENABLE(dot,
     [AS_HELP_STRING([--enable-dot],
        [use 'dot' to generate graphs in doxygen (auto)])])
   if test "x$enable_doxygen" = xyes; then
     AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
     test x$DOXYGEN = x && AC_MSG_ERROR([could not find doxygen])
     AC_PATH_PROG(DOT, dot, , $PATH)
     test x$DOT = x -a "x$enable_dot" = xyes && AC_MSG_ERROR([could not find dot])
   fi
   AM_CONDITIONAL(ENABLE_DOXYGEN, test "x$DOXYGEN" != "x")])

# Actually perform the doxygen check
BB_ENABLE_DOXYGEN

AC_CONFIG_FILES([Makefile \
                 bin/Makefile \
                 src/Makefile \
                 ringtones/Makefile \
                 test/Makefile\
                 test/sip/Makefile
                 test/unitTest/Makefile \
                 man/Makefile \
                 doc/Makefile \
                 doc/doxygen/Makefile
                 jami.pc])

dnl Transfer the Automake variables set here to Makefile.in.
    CPPFLAGS="$DAEMONCPPFLAGS"
    CXXFLAGS="$DAEMONCXXFLAGS"
    OBJCXX="$CXX $DAEMONCXXFLAGS"
    OBJCXXFLAG="$CXX $DAEMONCXXFLAGS"
    CFLAGS=$DAEMONCFLAGS
    LDFLAGS="$DAEMONLDFLAGS"
echo CFLAGS=$CFLAGS
echo CPPFLAGS=$CPPFLAGS
echo OBJCXX=$OBJCXX
echo OBJCXXFLAG=$OBJCXXFLAG
echo LDFLAGS=$LDFLAGS
echo DAEMONCPPFLAGS=$DAEMONCPPFLAGS
AC_SUBST([AM_CPPFLAGS], [$DAEMONCPPFLAGS])
AC_SUBST([AM_CFLAGS],   [$DAEMONCFLAGS])
AC_SUBST([AM_CXXFLAGS], [$DAEMONCXXFLAGS])
AC_SUBST([AM_LDFLAGS],  [$DAEMONLDFLAGS])

# Go!
AC_OUTPUT
