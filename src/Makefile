#
# Makefile for sflphone
# (c) 2004-2005 Savoir-faire Linux inc.
# Authors : Laurielle Lea (laurielle.lea@savoirfairelinux.com)
#           Jerome Oufella (jerome.oufella@savoirfairelinux.com)
#
CC  =  gcc
CXX	=  g++
MOC = $(QTDIR)/bin/moc
UIC = $(QTDIR)/bin/uic
PROGNAME = sflphone

BIN_DIR  = $(PREFIX)/bin
SHARE_DIR  = $(PREFIX)/share
PIXMAPS = pixmaps
SKINS = skins
RINGS = rings
METAL_SKINS = metal

# Platform-specific variables
HOSTMACH = $(shell uname -m | sed -e 's/ //g')
ifeq (${HOSTSYSTEM},PowerMacintosh)
	MACHOPTS=-mpowerpc
	HOSTDEFS=-DGUI_TEXT1 -DAUDIO_MACOSX
else
	HOSTSYS = $(shell uname -s)
endif

ifeq (${HOSTSYS},Linux)
	HOSTMACH = $(shell uname -m | sed -e 's/i.86/i386/')
	HOSTLIBS=-lX11 -lXext -lasound -lqt-mt
	# HOSTDEFS=-DGUI_TEXT1 -DAUDIO_OSS # eventually
	HOSTDEFS=-DGUI_QT -DAUDIO_ALSA
	ifeq (${HOSTMACH},i386)
		MACHOPTS=-march=i386
	endif
endif

INCPATH  = -I. -I$(QTDIR)/include -I$(CCPPDIR)/include/cc++2 -I$(CCRTPDIR)/include -I$(OSIPDIR)/include -I$(EXOPSIP)/include -I/usr/include -Iaudio/gsm -Iaudio/portaudio/pa_common 
CXXFLAGS = -pipe -Wall -W -g $(MACHOPTS) -pipe -DQT_NO_DEBUG -DQT_SHARED -DQT_THREAD_SUPPORT -fpermissive -Wno-deprecated $(INCPATH)

LIBS=-L$(QTDIR)/lib -L/usr/X11R6/lib -lm -L/opt/lib -losip2 -leXosip -lccrtp1 $(HOSTLIBS) `$(CCPPDIR)/bin/ccgnu2-config --flags --stdlibs` 

EXTRALIBS=audio/gsm/libgsm.a audio/portaudio/lib/libportaudio.a

CONFIGURE_CONF=$(shell ls ../configure.conf)

	
OBJS = \
	audio/audiobuffer.o \
	audio/audiocodec.o \
	audio/codecDescriptor.o \
	audio/audiodrivers.o \
	audio/audiodriversalsa.o \
	audio/audiodriversoss.o \
	audio/audiodriversportaudio.o \
	audio/audiortp.o \
	audio/dtmf.o \
	audio/dtmfgenerator.o \
	audio/tonegenerator.o \
	audio/g711.o \
	audio/gsmcodec.o \
	audio/alaw.o \
	audio/ulaw.o \
	call.o \
	configitem.o \
	configuration.o \
	configurationtree.o \
	error.o \
	eventthread.o \
	gui/guiframework.o \
	gui/qt/mydisplay.o \
    gui/qt/numerickeypadtools.o \
    gui/qt/phoneline.o \
    gui/qt/point.o \
	gui/qt/transqwidget.o \
    gui/qt/trayicon.o \
	gui/qt/trayicon.moc.o \
	gui/qt/trayicon_x11.o \
	gui/qt/vector.o \
	gui/qt/volumecontrol.o \
	gui/qt/volumecontrol.moc.o \
    gui/qt/jpushbutton.o \
	gui/qt/jpushbutton.moc.o \
    gui/qt/numerickeypad.o \
	gui/qt/numerickeypad.moc.o \
	gui/qt/qjlistboxpixmap.o \
    gui/qt/qtGUImainwindow.o \
	gui/qt/qtGUImainwindow.moc.o \
	gui/qt/configurationpanelui.o \
	gui/qt/configurationpanelui.moc.o \
	gui/qt/url_inputui.o \
	gui/qt/url_inputui.moc.o \
	main.o \
	manager.o \
	sipcall.o \
	sipvoiplink.o \
	skin.o \
	voIPLink.o \
	stun.o \
	udp.o 
  		
	
#start:	check prereq all
start:check all

check:
ifeq ($(CONFIGURE_CONF),../configure.conf)
  include ../configure.conf
else
  $(error You must run configure first !)
endif

.cpp.o:
	$(CXX) $(DEFVARS) -DAUDIO_PORTAUDIO $(CXXFLAGS) -c -o $@ $<

prereq:	gui/qt/url_input.ui gui/qt/configurationpanel.ui 
	@echo "Making User Interface files..."
	$(UIC) -o gui/qt/url_inputui.h gui/qt/url_input.ui
	$(UIC) -o gui/qt/url_inputui.cpp -impl gui/qt/url_inputui.h gui/qt/url_input.ui
	$(UIC) -o gui/qt/configurationpanelui.h gui/qt/configurationpanel.ui
	$(UIC) -o gui/qt/configurationpanelui.cpp -impl gui/qt/configurationpanelui.h gui/qt/configurationpanel.ui

%.moc.cpp: %.h
	$(MOC) -o $@ $<

	
stun.o: ../stund/stun.cxx ../stund/udp.h ../stund/stun.h
	$(CXX) -c $(CXXFLAGS) -o stun.o ../stund/stun.cxx

udp.o: ../stund/udp.cxx ../stund/udp.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o udp.o ../stund/udp.cxx
	
	
all: $(PROGNAME)
	
$(PROGNAME): $(OBJS) $(EXTRALIBS)
	$(CXX) -o $@ $(OBJS) $(LIBS) $(EXTRALIBS)
	
install:	all
	mkdir -p $(BIN_DIR)
	mkdir -p $(SHARE_DIR)/$(PROGNAME)
	install --mode=0755 $(PROGNAME) $(BIN_DIR)
	cd ..; cp -R $(PIXMAPS) $(SHARE_DIR)/$(PROGNAME)/ ; \
		chmod -R a+rX $(SHARE_DIR)/$(PROGNAME)/$(PIXMAPS)
	cd ..; cp -R $(SKINS) $(SHARE_DIR)/$(PROGNAME)/ ; \
		chmod -R a+rX $(SHARE_DIR)/$(PROGNAME)/$(SKINS)
	cd ..; cp -R $(RINGS) $(SHARE_DIR)/$(PROGNAME)/ ; \
		chmod -R a+rX $(SHARE_DIR)/$(PROGNAME)/$(RINGS)

uninstall: 
	rm -f $(BIN_DIR)/$(PROGNAME)
	rm -rf $(SHARE_DIR)/$(PROGNAME)

clean:
	rm -f $(PROGNAME) *.o audio/*.o audio/gsm/*.o gui/*.o gui/qt/*.o audio/gsm/*.a *~ *.moc.cpp *.bak core.*

