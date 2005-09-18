#bin_PROGRAMS = sflphonegui
noinst_PROGRAMS = servertest

#configurationpanel.h configurationpanelui.cpp configurationpanelmoc.cpp 
BUILT_SOURCESsss = \
	url_input.h \
	url_inputui.cpp \
	url_inputmoc.cpp \
	jpushbuttonmoc.cpp \
	trayiconmoc.cpp\
	volumecontrolmoc.cpp\
	numerickeypadmoc.cpp \
	mydisplaymoc.cpp \
	qtGUImainwindowmoc.cpp

servertest_SOURCES = \
	Account.cpp \
	AnswerReceiver.cpp \
	Call.cpp \
	main.cpp \
	Request.cpp \
	RequesterImpl.cpp \
	Session.cpp \
	SessionIO.cpp

SOURCES_GUI = \
	$(BUILT_SOURCES) \
	jpushbutton.cpp \
	mydisplay.cpp \
	numerickeypad.cpp \
	numerickeypadtools.cpp \
	trayicon.cpp \
	phoneline.cpp \
	point.cpp \
	qjlistboxpixmap.cpp \
	qtGUImainwindow.cpp \
	transqwidget.cpp \
	trayicon_x11.cpp \
	vector.cpp \
	volumecontrol.cpp 

CLEANFILES = \
	$(BUILT_SOURCES)

AM_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" 

AM_LDFLAGS = $(QT_LDFLAGS) $(X_LDFLAGS) -static
LDADD =  ../../../utilspp/libutilspp.la -lpthread $(LIBQT)
KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = $(QT_INCLUDES) $(X_INCLUDES) -I$(top_srcdir)/src


%.h: %.ui
	$(UIC) -o $@ $<

%moc.cpp: %.h
	$(MOC) -o $@ $<

%ui.cpp: %.ui
	$(UIC) -o $@ -impl $*.h $<
