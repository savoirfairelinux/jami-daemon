bin_PROGRAMS = sflphone

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

sflphone_SOURCES = call.cpp 	eventthread.cpp 	error.cpp 	main.cpp 	sipvoiplink.cpp 	voIPLink.cpp \
		sipcall.cpp 	managerimpl.cpp 	managerimpl.h 	manager.h 	global.h \
		eventthread.h 	error.h \
		sipvoiplink.h 	user_cfg.h 	call.h 	voIPLink.h 	sipcall.h 	\
		observer.cpp

sflphone_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(ZEROCONFFLAGS)

sflphone_LDFLAGS = -static
sflphone_LDADD =  gui/libguiframework.la audio/libaudio.la ../stund/libstun.la ../utilspp/libutilspp.la -lpthread $(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) config/libconfig.la 
KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = -I$(top_srcdir) -I$(srcdir)/audio/pacpp/include  $(libccext2_CFLAGS) $(libccgnu2_CFLAGS) $(portaudio_CFLAGS)

noinst_HEADERS = observer.h
