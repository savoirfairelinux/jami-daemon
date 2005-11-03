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
		sipcall.cpp 	managerimpl.cpp 	managerimpl.h 	manager.h 	global.h \
		eventthread.h 	\
		sipvoiplink.h 	user_cfg.h 	call.h 	voIPLink.h 	sipcall.h 	\
		observer.cpp

sflphoned_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(ZEROCONFFLAGS)

#sflphoned_LDFLAGS = -static
sflphoned_LDADD =  gui/libguiframework.la audio/libaudio.la ../stund/libstun.la ../utilspp/libutilspp.la -lpthread $(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) config/libconfig.la 
#KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = -I$(top_srcdir) -I$(srcdir)/audio/pacpp/include  $(libccext2_CFLAGS) $(libccgnu2_CFLAGS) $(portaudio_CFLAGS)

noinst_HEADERS = observer.h
