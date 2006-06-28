# LIBCURL_CHECK_CONFIG ([DEFAULT-ACTION], [MINIMUM-VERSION],
#                       [ACTION-IF-YES], [ACTION-IF-NO])
# ----------------------------------------------------------
#      David Shaw <dshaw@jabberwocky.com>   Jun-21-2005
#      Jean-Philippe Barrette-LaPierre Jan-17-2005
#                 <jean-philippe.barrette-lapierre@savoirfairelinux.com>
#
# *GREATLY inspired from libcurl.m4 (curl.haxx.se)
#
# Checks for portaudio.  DEFAULT-ACTION is the string yes or no to
# specify whether to default to --with-portaudio or --without-portaudio.
# If not supplied, DEFAULT-ACTION is yes.  MINIMUM-VERSION is the
# minimum version of portaudio to accept.  Pass the version as a regular
# version number like 0.19.0. If not supplied, any version is
# accepted.  ACTION-IF-YES is a list of shell commands to run if
# libcurl was successfully found and passed the various tests.
# ACTION-IF-NO is a list of shell commands that are run otherwise.
# Note that using --without-portaudio does run ACTION-IF-NO.
#
# This macro defines HAVE_PORTAUDIO if a working libcurl setup is found,
# and sets @PORTAUDIO@ and @PORTAUDIO_CPPFLAGS@ to the necessary values.
# Other useful defines are PORTAUDIO_FEATURE_xxx where xxx are the
# various features supported by portaudio.  xxx are capitalized.  
# See the list of AH_TEMPLATEs at the top of the macro for the 
# complete list of possible defines.  Shell variables 
# $portaudio_feature_xxx is also defined to 'yes' for those 
# features that were found. Note that xxx keep the same capitalization 
# as in the portaudio-config list (e.g. it's "OSS" and not "oss").
#
# Users may override the detected values by doing something like:
# PORTAUDIO="-lportaudio" PORTAUDIO_CPPFLAGS="-I/usr/myinclude" ./configure
#
# For the sake of sanity, this macro assumes that any portaudio that is
# found is after version 0.19.0, the first version that included the
# portaudio-config script.  Note that it is very important for people
# packaging binary versions of portaudio to include this script!
# Without portaudio-config, we can only guess what protocols are available.

AC_DEFUN([PORTAUDIO_CHECK_CONFIG],
[
  AH_TEMPLATE([LIBCURL_FEATURE_OSS],[Defined if portaudio supports OSS])
  AH_TEMPLATE([LIBCURL_FEATURE_ALSA],[Defined if libcurl supports ALSA])

  AC_ARG_WITH(portaudio,
     AC_HELP_STRING([--with-portaudio=DIR],[look for the portaudio library in DIR]),
     [_portaudio_with=$withval],[_portaudio_with=ifelse([$1],,[yes],[$1])])

  if test "$_portaudio_with" != "no" ; then

     AC_PROG_AWK

     _portaudio_version_parse="eval $AWK '{split(\$NF,A,\".\"); X=256*256*A[[1]]+256*A[[2]]+A[[3]]; print X;}'"

     _portaudio_try_link=yes

     if test -d "$_portaudio_with" ; then
        CPPFLAGS="${CPPFLAGS} -I$withval/include"
        LDFLAGS="${LDFLAGS} -L$withval/lib"
     fi

     AC_PATH_PROG([_portaudio_config],[portaudio-config])

     if test x$_portaudio_config != "x" ; then
        AC_CACHE_CHECK([for the version of portaudio],
	   [portaudio_cv_lib_portaudio_version],
           [portaudio_cv_lib_portaudio_version=`$_portaudio_config --version | $AWK '{print $[]2}'`])

	_portaudio_version=`echo $portaudio_cv_lib_portaudio_version | $_portaudio_version_parse`
	_portaudio_wanted=`echo ifelse([$2],,[0],[$2]) | $_portaudio_version_parse`

        if test $_portaudio_wanted -gt 0 ; then
	   AC_CACHE_CHECK([for portaudio >= version $2],
	      [portaudio_cv_lib_version_ok],
              [
   	      if test $_portaudio_version -ge $_portaudio_wanted ; then
	         portaudio_cv_lib_version_ok=yes
      	      else
	         portaudio_cv_lib_version_ok=no
  	      fi
              ])
        fi

	if test $_portaudio_wanted -eq 0 || test x$portaudio_cv_lib_version_ok = xyes ; then
           if test x"$PORTAUDIO_CPPFLAGS" = "x" ; then
              PORTAUDIO_CPPFLAGS=`$_portaudio_config --cflags`
           fi
           if test x"$PORTAUDIO" = "x" ; then
              PORTAUDIO=`$_portaudio_config --libs`

              # This is so silly, but Apple actually has a bug in their
	      # portaudio-config script.  Fixed in Tiger, but there are still
	      # lots of Panther installs around.
              case "${host}" in
                 powerpc-apple-darwin7*)
                    PORTAUDIO=`echo $PORTAUDIO | sed -e 's|-arch i386||g'`
                 ;;
              esac
           fi

	   # All portaudio-config scripts support --feature
	   _portaudio_features=`$_portaudio_config --feature`
	else
           _portaudio_try_link=no
	fi

	unset _portaudio_wanted
     fi

     if test $_portaudio_try_link = yes ; then

        # we didn't find portaudio-config, so let's see if the user-supplied
        # link line (or failing that, "-lportaudio") is enough.
        PORTAUDIO=${PORTAUDIO-"-lportaudio"}

        AC_CACHE_CHECK([whether portaudio is usable],
           [portaudio_cv_lib_portaudio_usable],
           [
           _portaudio_save_cppflags=$CPPFLAGS
           CPPFLAGS="$CPPFLAGS $PORTAUDIO_CPPFLAGS"
           _portaudio_save_libs=$LIBS
           LIBS="$LIBS $PORTAUDIO"

           AC_LINK_IFELSE(
             [AC_LANG_PROGRAM(
               [#include <portaudio.h>],
               [[
/* Try and use a few common options to force a failure
   if we are missing symbols or can't link. */
Pa_Initialize();
               ]])],
             [portaudio_cv_lib_portaudio_usable=yes],
             [portaudio_cv_lib_portaudio_usable=no])

           CPPFLAGS=$_portaudio_save_cppflags
           LIBS=$_portaudio_save_libs
           unset _portaudio_save_cppflags
           unset _portaudio_save_libs
           ])

        if test $portaudio_cv_lib_portaudio_usable = yes ; then

	   # Does curl_free() exist in this version of portaudio?
	   # If not, fake it with free()

           AC_DEFINE(HAVE_PORTAUDIO,1,
             [Define to 1 if you have a functional portaudio library.])
           AC_SUBST(PORTAUDIO_CPPFLAGS)
           AC_SUBST(PORTAUDIO)

           for _portaudio_feature in $_portaudio_features ; do
	      AC_DEFINE_UNQUOTED(AS_TR_CPP(portaudio_feature_$_portaudio_feature),[1])
	      eval AS_TR_SH(portaudio_feature_$_portaudio_feature)=yes
           done

	   if test "x$_portaudio_protocols" = "x" ; then

	      # We don't have --protocols, so just assume that all
	      # protocols are available
	      _portaudio_protocols="HTTP FTP GOPHER FILE TELNET LDAP DICT"

	      if test x$portaudio_feature_SSL = xyes ; then
	         _portaudio_protocols="$_portaudio_protocols HTTPS"

		 # FTPS wasn't standards-compliant until version
		 # 7.11.0
		 if test $_portaudio_version -ge 461568; then
		    _portaudio_protocols="$_portaudio_protocols FTPS"
		 fi
	      fi
	   fi

	   for _portaudio_protocol in $_portaudio_protocols ; do
	      AC_DEFINE_UNQUOTED(AS_TR_CPP(portaudio_protocol_$_portaudio_protocol),[1])
	      eval AS_TR_SH(portaudio_protocol_$_portaudio_protocol)=yes
           done
        fi
     fi

     unset _portaudio_try_link
     unset _portaudio_version_parse
     unset _portaudio_config
     unset _portaudio_feature
     unset _portaudio_features
     unset _portaudio_protocol
     unset _portaudio_protocols
     unset _portaudio_version
  fi

  if test x$_portaudio_with = xno || test x$portaudio_cv_lib_portaudio_usable != xyes ; then
     # This is the IF-NO path
     ifelse([$4],,:,[$4])
  else
     # This is the IF-YES path
     ifelse([$3],,:,[$3])
  fi

  unset _portaudio_with
])dnl


#This macro is used when you compile portaudio.
AC_DEFUN([PORTAUDIO_SETUP],[
AC_REQUIRE([AC_CANONICAL_HOST])

CFLAGS_save=$CFLAGS
LIBS_save=$LIBS
CXXFLAGS_save=$CXXFLAGS

AC_ARG_WITH(alsa, 
            [  --with-alsa (default=yes)],
            with_alsa=$withval, with_alsa="yes")

AC_ARG_WITH(jack, 
            [  --with-jack (default=no)],
            with_jack=$withval, with_jack="no")

AC_ARG_WITH(oss, 
            [  --with-oss (default=yes)],
            with_oss=$withval, with_oss="yes")

AC_ARG_WITH(host_os, 
            [  --with-host_os (no default)],
            host_os=$withval)

AC_ARG_WITH(winapi,
            [  --with-winapi ((wmme/directx/asio) default=wmme)],
            with_winapi=$withval, with_winapi="wmme")

dnl Mac API added for ASIO, can have other api's listed
AC_ARG_WITH(macapi,
            [  --with-macapi ((asio/core/sm) default=core)],
            with_macapi=$withval, with_macapi="core")

AC_ARG_WITH(asiodir,
            [  --with-asiodir (default=/usr/local/asiosdk2)],
            with_asiodir=$withval, with_asiodir="/usr/local/asiosdk2")

AC_ARG_WITH(dxdir,
            [  --with-dxdir (default=/usr/local/dx7sdk)],
            with_dxdir=$withval, with_dxdir="/usr/local/dx7sdk")
               
dnl This must be one of the first tests we do or it will fail...
AC_C_BIGENDIAN

dnl checks for various host APIs and arguments to configure that
dnl turn them on or off
AC_CHECK_LIB(asound, snd_pcm_open, have_alsa=yes, have_alsa=no)

if test x$with_jack = xyes; then
PKG_CHECK_MODULES(JACK, jack, have_jack=yes, have_jack=no)
fi

dnl sizeof checks: we will need a 16-bit and a 32-bit type

AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)

dnl extra variables
AC_SUBST(PADLL)
AC_SUBST(SHARED_FLAGS)
AC_SUBST(DLL_LIBS)
AC_SUBST(CXXFLAGS)
AC_SUBST(NASM)
AC_SUBST(NASMOPT)


CFLAGS="$CFLAGS -Wall -pedantic -pipe -fPIC"

if test x$ac_cv_c_bigendian = xyes  ; then
   CFLAGS="$CFLAGS -DPA_BIG_ENDIAN"
else
   CFLAGS="$CFLAGS -DPA_LITTLE_ENDIAN"
fi


case $host in
  *darwin*)
	dnl Mac OS X configuration

	CFLAGS="$CFLAGS -DPA_USE_COREAUDIO"
	compile_with_darwin=yes;
	LIBS="-framework CoreAudio -framework AudioToolbox";
        if test x$with_macapi = x"asio"; then
	    compile_with_asio=yes
            if test x$with_asiodir = xyes; then
              ASIODIR="$with_asiodir";
            else
              ASIODIR="/usr/local/asiosdk2";
            fi
            CFLAGS="$CFLAGS -Ipa_asio -I$ASIDIR/host/mac -I$ASIODIR/common";
        fi
	;;

   *)	
	dnl Unix configuration
	AC_CHECK_LIB(pthread, pthread_create,,AC_MSG_ERROR([libpthread not found!]))

	if test x$have_alsa = xyes  -a  x$with_alsa != xno  ; then
		compile_with_alsa=yes
		LIBS="$LIBS -lasound"
		DLL_LIBS="$DLL_LIBS -lasound"
		CFLAGS="$CFLAGS -DPA_USE_ALSA"
	fi
	
	
	if test x$have_jack = xyes -a x$with_jack != xno ; then
		compile_with_jack=yes
		LIBS="$LIBS $JACK_LIBS"
		DLL_LIBS="$DLL_LIBS $JACK_LIBS"
		CFLAGS="$CFLAGS $JACK_CFLAGS"
		CFLAGS="$CFLAGS -DPA_USE_JACK"
	fi

	if test x$with_oss != xno  ; then
		compile_with_oss=yes
		CFLAGS="$CFLAGS -DPA_USE_OSS"
	fi

	LIBS="$LIBS -lm -lpthread";
	compile_with_unix=yes
	CFLAGS="$CFLAGS -DPA_USE_UNIX"
esac

AM_CONDITIONAL(ENABLE_ASIO, test x$compile_with_asio = xyes)
AM_CONDITIONAL(ENABLE_UNIX, test x$compile_with_unix = xyes)
AM_CONDITIONAL(ENABLE_DARWIN, test x$compile_with_darwin = xyes)
AM_CONDITIONAL(ENABLE_OSS, test x$compile_with_oss = xyes)
AM_CONDITIONAL(ENABLE_JACK, test x$compile_with_jack = xyes)
AM_CONDITIONAL(ENABLE_ALSA, test x$compile_with_alsa = xyes)
	
PORTAUDIO_CFLAGS=$CFLAGS
PORTAUDIO_LIBS=$LIBS
	
CFLAGS=$CFLAGS_save
LIBS=$LIBS_save
CXXFLAGS=$CXXFLAGS_save

AC_SUBST(PORTAUDIO_CFLAGS)
AC_SUBST(PORTAUDIO_LIBS)
AC_SUBST(PORTAUDIO)
AC_SUBST(ASIODIR)

])

