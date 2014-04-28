include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient.la

BUILT_SOURCES=                      \
    callmanager-glue.h              \
    configurationmanager-glue.h     \
    instance-glue.h

if SFL_PRESENCE
BUILT_SOURCES+=presencemanager-glue.h
presencemanager-glue.h: presencemanager-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@
endif

if SFL_VIDEO
BUILT_SOURCES+=video_controls-glue.h
video_controls-glue.h: video_controls-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@
endif

# Rule to generate the binding headers
callmanager-glue.h: callmanager-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@

# Rule to generate the binding headers
configurationmanager-glue.h: configurationmanager-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@

# Rule to generate the binding headers
instance-glue.h: instance-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@

libclient_la_SOURCES = \
    callmanager_dbus.cpp \
    ../callmanager.cpp \
    configurationmanager_dbus.cpp  \
    ../configurationmanager.cpp  \
    instance.cpp  \
    client.cpp

if SFL_PRESENCE
libclient_la_SOURCES+=../presencemanager.cpp presencemanager_dbus.cpp
endif

if SFL_VIDEO
libclient_la_SOURCES+=video_controls.cpp
endif

if USE_NETWORKMANAGER
libclient_la_SOURCES += networkmanager.cpp
NETWORKMANAGER=-DUSE_NETWORKMANAGER
endif

libclient_la_CXXFLAGS = -I../ \
            -DPREFIX=\"$(prefix)\" \
            -DPROGSHAREDIR=\"${datadir}/sflphone\" \
            $(NETWORKMANAGER) \
            $(DBUSCPP_CFLAGS)

noinst_HEADERS =            \
    ../callmanager.h        \
    ../configurationmanager.h  \
    instance.h              \
    ../client.h             \
    networkmanager_proxy.h  \
    networkmanager.h        \
    dbus_cpp.h        		\
    $(BUILT_SOURCES)

if SFL_PRESENCE
noinst_HEADERS+=../presencemanager.h
endif

if SFL_VIDEO
noinst_HEADERS+=../video_controls.h
endif

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.sflphone.SFLphone.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	sed -e "s|libexec|$(libdir)/sflphone|" $<> $@

EXTRA_DIST = README $(service_in_files) \
			 callmanager-introspec.xml \
			 configurationmanager-introspec.xml \
			 instance-introspec.xml \
			 org.freedesktop.NetworkManager.xml

if SFL_PRESENCE
EXTRA_DIST += presencemanager-introspec.xml
endif

if SFL_VIDEO
EXTRA_DIST += video_controls-introspec.xml
endif

CLEANFILES= \
    $(BUILT_SOURCES) \
	org.sflphone.SFLphone.service
