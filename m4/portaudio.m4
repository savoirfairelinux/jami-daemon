AC_DEFUN([LP_SETUP_PORTAUDIO],[
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
PORTAUDIO_CXXFLAGS=$CXXFLAGS
	
CFLAGS=$CFLAGS_save
LIBS=$LIBS_save
CXXFLAGS=$CXXFLAGS_save

AC_SUBST(PORTAUDIO_CFLAGS)
AC_SUBST(PORTAUDIO_CXXFLAGS)
AC_SUBST(PORTAUDIO_LIBS)
AC_SUBST(ASIODIR)

])

