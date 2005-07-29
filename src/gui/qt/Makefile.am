noinst_LTLIBRARIES = libsflphoneqt.la

BUILT_SOURCES = \
	configurationpanel.h\
	configurationpanelui.cpp\
	configurationpanelmoc.cpp\
	url_input.h \
	url_inputui.cpp \
	url_inputmoc.cpp \
	jpushbuttonmoc.cpp \
	trayiconmoc.cpp\
	volumecontrolmoc.cpp\
	numerickeypadmoc.cpp \
	mydisplaymoc.cpp \
	qtGUImainwindowmoc.cpp

libsflphoneqt_la_SOURCES = \
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

libsflphoneqt_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(portaudio_CFLAGS)
libsflphoneqt_la_LIBADD = $(portaudio_LIBS)

KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = -I../../audio/pacpp/include/ $(QT_INCLUDES) $(X_INCLUDES) $(libccext2_CFLAGS) $(portaudio_CFLAGS)


%.h: %.ui
	$(UIC) -o $@ $<

%moc.cpp: %.h
	$(MOC) -o $@ $<

%ui.cpp: %.ui
	$(UIC) -o $@ -impl $*.h $<


