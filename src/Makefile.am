libexec_PROGRAMS = sflphoned
bin_SCRIPTS = sflphone

if USE_ZEROCONF
ZEROCONFDIR = zeroconf
ZEROCONFLIB = zeroconf/libzeroconf.la
ZEROCONFFLAGS = -DUSE_ZEROCONF
else
ZEROCONFDIR =
ZEROCONFLIB =
ZEROCONFFLAGS =  
endif

SUBDIRS = audio config gui $(ZEROCONFDIR)

sflphoned_SOURCES = call.cpp 	eventthread.cpp 	main.cpp 	sipvoiplink.cpp 	voIPLink.cpp \
		sipcall.cpp 	managerimpl.cpp	\
		observer.cpp

sflphoned_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(ZEROCONFFLAGS)

#sflphoned_LDFLAGS = -static
sflphoned_LDADD =  ./libsflphone.la $(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) 
#KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/audio/pacpp/include  $(libccext2_CFLAGS) $(libccgnu2_CFLAGS) $(portaudio_CFLAGS) -I$(top_srcdir)/portaudio/pa_common

libsflphone_la_LDFLAGS= -version-info 0:1:0
libsflphone_la_LIBADD = \
	$(top_builddir)/exosip2/libeXosip2.la \
	$(top_builddir)/portaudio/libportaudio.la \
	$(top_builddir)/stund/libstun.la \
	$(top_builddir)/utilspp/libutilspp.la \
	./audio/libaudio.la \
	./gui/libguiframework.la \
	./config/libconfig.la 

libsflphone_la_SOURCES =

noinst_LTLIBRARIES = libsflphone.la
noinst_HEADERS = managerimpl.h manager.h global.h observer.h eventthread.h sipvoiplink.h user_cfg.h \
                 call.h voIPLink.h sipcall.h
