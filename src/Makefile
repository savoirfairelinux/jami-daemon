#
# Makefile for sflphone
# (c) 2004 Savoir-faire Linux inc.
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
METAL_SKINS = metal

INC_OTHERS = ../stund

INCPATH  = -I. -I$(QTDIR)/include -I$(CCPPDIR)/include/cc++2 -I$(CCRTPDIR)/include -I$(OSIPDIR)/include -I$(EXOPSIP)/include -I/usr/include

CXXFLAGS = -pipe -Wall -W -g -pipe -march=i386 -mcpu=i686  -DQT_NO_DEBUG -DQT_SHARED -DQT_THREAD_SUPPORT -fpermissive -Wno-deprecated $(INCPATH)

LIBS     = -L$(QTDIR)/lib -L/usr/X11R6/lib -lqt-mt -lXext -lX11 -lm -L/opt/lib -losip2 -leXosip -lccrtp1 `ccgnu2-config --flags --stdlibs`

CONFIGURE_CONF=$(shell ls ../configure.conf)
	
OBJS = \
	audiobuffer.o \
	audiocodec.o \
	audiodrivers.o \
	audiodriversoss.o \
	audiortp.o \
	configuration.o \
	configitem.o \
	configurationtree.o \
	dtmf.o \
	dtmfgenerator.o \
	g711.o \
	main.o \
	manager.o \
	mydisplay.o \
	numerickeypadtools.o \
	phoneline.o \
	point.o \
	qjlistboxpixmap.o \
	sip.o \
	sipcall.o \
	skin.o \
	url_inputui.o url_inputui.moc.o \
	phonebookui.o phonebookui.moc.o \
	configurationpanelui.o configurationpanelui.moc.o \
	tonegenerator.o \
	transqwidget.o \
	trayicon.o trayicon.moc.o trayicon_x11.o \
	jpushbutton.o jpushbutton.moc.o \
	numerickeypad.o numerickeypad.moc.o \
	qtGUImainwindow.o qtGUImainwindow.moc.o \
	stun.o udp.o
	
start:	check prereq all

check:
ifeq ($(CONFIGURE_CONF),../configure.conf)
  include ../configure.conf
else
  $(error You must run configure first !)
endif

.cpp.o:
	$(CXX) $(DEFVARS) $(CXXFLAGS) -c -o $@ $<

prereq:	phonebook.ui url_input.ui configurationpanel.ui
	@echo "Making User Interface files..."
	$(UIC) -o phonebookui.h phonebook.ui
	$(UIC) -o phonebookui.cpp -impl phonebookui.h phonebook.ui
	$(UIC) -o url_inputui.h url_input.ui
	$(UIC) -o url_inputui.cpp -impl url_inputui.h url_input.ui
	$(UIC) -o configurationpanelui.h configurationpanel.ui
	$(UIC) -o configurationpanelui.cpp -impl configurationpanelui.h configurationpanel.ui

%.moc.cpp: %.h
	$(MOC) -o $@ $<

	
stun.o: ../stund/stun.cxx ../stund/udp.h ../stund/stun.h
	$(CXX) -c $(CXXFLAGS) -o stun.o ../stund/stun.cxx

udp.o: ../stund/udp.cxx ../stund/udp.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o udp.o ../stund/udp.cxx
	
	
all: $(PROGNAME)

$(PROGNAME): $(OBJS) 
	$(CXX) -o $@ $(OBJS) $(LIBS)
	
install:	all
	mkdir -p $(BIN_DIR)
	mkdir -p $(SHARE_DIR)/$(PROGNAME)
	install --mode=0755 $(PROGNAME) $(BIN_DIR)
	cd ..; cp -R $(PIXMAPS) $(SHARE_DIR)/$(PROGNAME)/ ; \
		chmod -R a+rX $(SHARE_DIR)/$(PROGNAME)/$(PIXMAPS)
	cd ..; cp -R $(SKINS) $(SHARE_DIR)/$(PROGNAME)/ ; \
		chmod -R a+rX $(SHARE_DIR)/$(PROGNAME)/$(SKINS)

uninstall: 
	rm -f $(BIN_DIR)/$(PROGNAME)
	rm -rf $(SHARE_DIR)/$(PROGNAME)

clean:
	rm -f $(PROGNAME) *.o *.a *~ *.moc.cpp *.bak core.*

