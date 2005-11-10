AC_DEFUN([LP_CHECK_SPEEX],[
dnl only accept speex>=1.1.6 or 1.0.5 (the versions that have speex_encode_int )
AC_ARG_WITH( speex,
      [  --with-speex      Set prefix where speex lib can be found (ex:/usr, /usr/local) [default=/usr] ],
      [ speex_prefix=${withval}],[ speex_prefix="/usr" ])

SPEEX_CFLAGS=" -I${speex_prefix}/include -I${speex_prefix}/include/speex"
SPEEX_LIBS="-L${speex_prefix}/lib -lspeex -lm"
CPPFLAGS_save=$CPPFLAGS
CPPFLAGS=$SPEEX_CFLAGS
LDFLAGS_save=$LDFLAGS
LDFLAGS=$SPEEX_LIBS
AC_CHECK_HEADERS(speex.h,[AC_CHECK_LIB(speex,speex_encode_int,speex_found=yes,speex_found=no)
],speex_found=no)

if test "$speex_found" = "no" ; then
AC_MSG_ERROR([Could not find a libspeex version that have the speex_encode_int() function. Please install libspeex=1.0.5 or libspeex>=1.1.6])
fi

AC_SUBST(SPEEX_CFLAGS)
AC_SUBST(SPEEX_LIBS)
CPPFLAGS=$CPPFLAGS_save
LDFLAGS=$LDFLAGS_save
])
