/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
systems. This function is required for `alloca.c' support on those systems.
*/
/* #undef CRAY_STACKSEG_END */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Define to 1 if you want hardware acceleration support. */
#define RING_ACCEL 1

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
*/
#define HAVE_ALLOCA_H 1

/* Define if you have alsa */
#define HAVE_ALSA 0

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 0

/* Define if you have CoreAudio */
#define HAVE_COREAUDIO 0

/* define if the compiler supports basic C++11 syntax */
#define HAVE_CXX11 1

/* Define to enable dht */
#define HAVE_DHT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define if you have libiax */
#define HAVE_IAX 0

/* Define if you have instant messaging support */
#define HAVE_INSTANT_MESSAGING 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have jack */
#define HAVE_JACK 0

/* Define to 1 if you have the <libintl.h> header file. */
#define HAVE_LIBINTL_H 0

/* Define if you have libupnp */
#define HAVE_LIBUPNP 1

/* Define if you have natpmp */
#define HAVE_LIBNATPMP 0

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define if you have OpenSL */
/* #undef HAVE_OPENSL */

/* Define if you have portaudio */
#define HAVE_PORTAUDIO 1

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* Have PTHREAD_PRIO_INHERIT. */
#define HAVE_PTHREAD_PRIO_INHERIT 1

/* Define to 1 if the system has the type `ptrdiff_t'. */
/* #undef HAVE_PTRDIFF_T */

/* Define if you have pulseaudio */
#define HAVE_PULSE 0

/* Define if you have sdes support */
#define HAVE_SDES 1

/* Define if you have shared memory support */
#define HAVE_SHM 0

/* Define if you have libspeex */
#define HAVE_SPEEX 0

/* Define if you have libspeexdsp */
#define HAVE_SPEEXDSP 0

/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 0

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if you have tls support */
#define HAVE_TLS 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "jami"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "jami@lists.savoirfairelinux.net"

/* Define to the full name of this package. */
#define PACKAGE_NAME "Jami"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "Jami"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "jami"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.3.0"

/* Define to necessary symbol if this constant uses a non-standard name on
your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* Video support enabled */
#define ENABLE_VIDEO /**/

/* Name directory service support enabled */
#define HAVE_RINGNS 1

/* If using the C implementation of alloca, define if you know the
direction of stack growth for your system; otherwise it will be
automatically deduced at runtime.
STACK_DIRECTION > 0 => grows toward higher addresses
STACK_DIRECTION < 0 => grows toward lower addresses
STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if the `S_IS*' macros in <sys/stat.h> do not work properly. */
/* #undef STAT_MACROS_BROKEN */

/* Define to 1 if you have the ANSI C header files. */
/* #undef STDC_HEADERS */

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 for Unicode (Wide Chars) APIs. */
#define UNICODE 1
#undef _MBCS

/* Version number of package */
#define VERSION "2.3.0"

// UWP compatibility
#define PROGSHAREDIR ""

/* Define to limit the scope of <windows.h>. */
/* #undef WIN32_LEAN_AND_MEAN */

/* ISO C, POSIX, and 4.3BSD things. */
/* #undef _BSD_SOURCE */

/* Extensions to ISO C99 from ISO C11. */
/* #undef _ISOC11_SOURCE */

/* Extensions to ISO C89 from ISO C99. */
/* #undef _ISOC99_SOURCE */

/* IEEE Std 1003.1. */
/* #undef _POSIX_C_SOURCE */

/* IEEE Std 1003.1. */
/* #undef _POSIX_SOURCE */

/* ISO C, POSIX, and SVID things. */
/* #undef _SVID_SOURCE */

/* Define to 1 for Unicode (Wide Chars) APIs. */
/* #undef _UNICODE */

/* POSIX and XPG 7th edition */
/* #undef _XOPEN_SOURCE */

/* XPG things and X/Open Unix extensions. */
/* #undef _XOPEN_SOURCE_EXTENDED */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to empty if the keyword `volatile' does not work. Warning: valid
code using `volatile' can become incorrect without. Disable with care. */
/* #undef volatile */
