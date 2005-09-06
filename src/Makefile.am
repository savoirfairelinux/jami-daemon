bin_PROGRAMS = sflphone

if MAINTENER_CODE
maintener_source = 
endif

if USE_ZEROCONF
SUBDIRS = audio gui zeroconf
ZEROCONFLIB = zeroconf/libzeroconf.la
ZEROCONFFLAGS = -DUSE_ZEROCONF
else
SUBDIRS = audio gui
ZEROCONFLIB =
ZEROCONFFLAGS =  
endif

sflphone_SOURCES = \
	configitem.cpp \
	configuration.cpp \
	configurationtree.cpp \
	call.cpp \
	eventthread.cpp \
	error.cpp \
	main.cpp \
	sipvoiplink.cpp \
	voIPLink.cpp \
	sipcall.cpp \
	skin.cpp \
	managerimpl.cpp \
	managerimpl.h \
	manager.h \
	global.h \
	configitem.h \
	configuration.h \
	configurationtree.h \
	eventthread.h \
	error.h \
	sipvoiplink.h \
	user_cfg.h \
	call.h \
	phonegi-bin.h \
	voIPLink.h \
	sipcall.h \
	skin.h \
	${maintener_source}




sflphone_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(ZEROCONFFLAGS)

sflphone_LDFLAGS = $(QT_LDFLAGS) $(X_LDFLAGS) -static
sflphone_LDADD =  gui/libguiframework.la audio/libaudio.la ../stund/libstun.la ../utilspp/libutilspp.la -lpthread $(LIBQT) $(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) 
KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = $(QT_INCLUDES) $(X_INCLUDES) -I$(top_srcdir) -Igui/qt -I$(srcdir)/audio/pacpp/include  $(libccext2_CFLAGS) $(libccgnu2_CFLAGS) $(portaudio_CFLAGS)

