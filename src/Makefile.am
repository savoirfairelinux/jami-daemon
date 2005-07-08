SUBDIRS = audio gui

bin_PROGRAMS = sflphone

sflphone_SOURCES = \
	configitem.cpp \
	configuration.cpp \
	configurationtree.cpp \
	manager.cpp \
	call.cpp \
	eventthread.cpp \
	error.cpp \
	main.cpp \
	sipvoiplink.cpp \
	voIPLink.cpp \
	sipcall.cpp \
	skin.cpp \
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
	skin.h 




sflphone_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\"

sflphone_LDFLAGS = $(QT_LDFLAGS) $(X_LDFLAGS)
sflphone_LDADD =  gui/libguiframework.la audio/libaudio.la ../stund/libstun.la -lpthread 


KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = $(QT_INCLUDES) $(X_INCLUDES) -Iaudio/pacpp/include





