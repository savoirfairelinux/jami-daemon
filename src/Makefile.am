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

if USE_IAX
IAX_LIBS = $(top_builddir)/libs/libiax2/src/libiax.la
IAX_FLAGS = -DUSE_IAX
IAX_CFLAGS = -I$(top_srcdir)/libs/libiax2/src/
IAXSOURCES = iaxaccount.cpp iaxvoiplink.cpp iaxcall.cpp
IAXHEADERS = iaxaccount.h iaxvoiplink.h iaxcall.h
else
IAX_LIBS = 
IAX_FLAGS =
IAX_CFLAGS =
IAXSOURCES =
IAXHEADERS =
endif

if USE_SAMPLERATE
SAMPLERATE_FLAG=-DUSE_SAMPLERATE
SAMPLERATE_LIB =-lsamplerate
else
SAMPLERATE_FLAG=
SAMPLERATE_LIB=
endif

SUBDIRS = audio config gui $(ZEROCONFDIR)

sflphoned_SOURCES = eventthread.cpp 	main.cpp 	voIPLink.cpp \
		managerimpl.cpp	observer.cpp \
		account.cpp sipaccount.cpp accountcreator.cpp \
                sipvoiplink.cpp call.cpp sipcall.cpp \
		$(IAXSOURCES)

sflphoned_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(ZEROCONFFLAGS) $(IAX_FLAGS) $(SFLPHONE_CFLAGS)

#sflphoned_LDFLAGS = -static
sflphoned_LDADD =  ./libsflphone.la $(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) $(IAX_LIBS) $(EXOSIP_LIBS)
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs $(libccext2_CFLAGS) $(libccgnu2_CFLAGS) $(IAX_CFLAGS) $(USER_INCLUDES) $(SAMPLERATE_FLAG)

#libsflphone_la_LDFLAGS= -version-info 0:1:0
libsflphone_la_LIBADD = \
	$(top_builddir)/libs/stund/libstun.la \
	$(top_builddir)/libs/utilspp/libutilspp.la \
	./audio/libaudio.la \
	./gui/libguiframework.la \
	./config/libconfig.la \
	$(IAX_LIBS) $(SAMPLERATE_LIB)

libsflphone_la_SOURCES =

noinst_LTLIBRARIES = libsflphone.la
noinst_HEADERS = managerimpl.h manager.h global.h observer.h eventthread.h user_cfg.h \
                 voIPLink.h \
		 account.h sipaccount.h accountcreator.h \
                 sipvoiplink.h  call.h  sipcall.h \
		 $(IAXHEADERS)

