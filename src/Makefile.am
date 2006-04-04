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

IAX_LIBS = $(top_builddir)/libs/libiax2/src/libiax.la
IAX_FLAGS = -DUSE_IAX
IAX_CFLAGS = -I$(top_srcdir)/libs/libiax2/src/
IAXSOURCES = iaxvoiplink.cpp
IAXHEADERS = iaxvoiplink.h

SUBDIRS = audio config gui $(ZEROCONFDIR)

sflphoned_SOURCES = eventthread.cpp 	main.cpp 	voIPLink.cpp \
		managerimpl.cpp	observer.cpp \
		account.cpp sipaccount.cpp iaxaccount.cpp accountcreator.cpp \
                sipvoiplink.cpp call.cpp sipcall.cpp \
		$(IAXSOURCES)

sflphoned_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(ZEROCONFFLAGS) $(IAX_FLAGS)

#sflphoned_LDFLAGS = -static
sflphoned_LDADD =  ./libsflphone.la $(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) $(IAX_LIBS)
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs -I$(srcdir)/audio/pacpp/include  $(libccext2_CFLAGS) $(libccgnu2_CFLAGS) $(portaudio_CFLAGS) -I$(top_srcdir)/libs/portaudio/pa_common $(IAX_CFLAGS)

#libsflphone_la_LDFLAGS= -version-info 0:1:0
libsflphone_la_LIBADD = \
	$(top_builddir)/libs/eXosip2/libeXosip2.la \
	$(top_builddir)/libs/stund/libstun.la \
	$(top_builddir)/libs/utilspp/libutilspp.la \
	$(top_builddir)/libs/portaudio/libportaudio.la \
	./audio/libaudio.la \
	./gui/libguiframework.la \
	./config/libconfig.la \
        $(IAX_LIBS)

libsflphone_la_SOURCES =

noinst_LTLIBRARIES = libsflphone.la
noinst_HEADERS = managerimpl.h manager.h global.h observer.h eventthread.h user_cfg.h \
                 voIPLink.h \
		 account.h sipaccount.h iaxaccount.h accountcreator.h \
                 sipvoiplink.h  call.h  sipcall.h \
		 $(IAXHEADERS)

