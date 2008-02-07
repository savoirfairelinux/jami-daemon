sflcodecdir = $(libdir)/sflphone/codecs
bin_PROGRAMS = sflphoned


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
# Use the global instead of locally built
IAX_LIBS = $(top_builddir)/libs/libiax2/src/libiax.la
#IAX_LIBS =
IAX_FLAGS = -DUSE_IAX
#IAX_CFLAGS = -I$(top_srcdir)/libs/libiax2/src/
IAX_CFLAGS =
IAXSOURCES = iaxaccount.cpp iaxvoiplink.cpp iaxcall.cpp
IAXHEADERS = iaxaccount.h iaxvoiplink.h iaxcall.h
else
IAX_LIBS = 
IAX_FLAGS =
IAX_CFLAGS =
IAXSOURCES =
IAXHEADERS =
endif

SUBDIRS = audio config dbus $(ZEROCONFDIR)

sflphoned_SOURCES = eventthread.cpp 	main.cpp 	voiplink.cpp \
		managerimpl.cpp	observer.cpp \
		account.cpp sipaccount.cpp accountcreator.cpp \
                sipvoiplink.cpp call.cpp sipcall.cpp \
		$(IAXSOURCES)

sflphoned_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(ZEROCONFFLAGS) $(IAX_FLAGS) $(SFLPHONE_CFLAGS) $(SIP_CFLAGS)

#sflphoned_LDFLAGS = -static
sflphoned_LDADD =  ./libsflphone.la $(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) $(IAX_LIBS) $(EXOSIP_LIBS) 
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs $(libccext2_CFLAGS) $(libccgnu2_CFLAGS) $(IAX_CFLAGS) $(USER_INCLUDES) $(libdbuscpp_CFLAGS) \
	      -DCODECS_DIR=\""$(sflcodecdir)"\" 


# libsflphone_la_LDFLAGS= -version-info 0:1:0
libsflphone_la_LIBADD = \
	$(top_builddir)/libs/stund/libstun.la \
	$(top_builddir)/libs/utilspp/libutilspp.la \
	./audio/libaudio.la \
	./dbus/libdbus.la \
	./config/libconfig.la \
	$(IAX_LIBS)

libsflphone_la_SOURCES =

noinst_LTLIBRARIES = libsflphone.la
noinst_HEADERS = managerimpl.h manager.h global.h observer.h eventthread.h user_cfg.h \
                 voiplink.h \
		 account.h sipaccount.h accountcreator.h \
                 sipvoiplink.h  call.h  sipcall.h \
		 $(IAXHEADERS)

