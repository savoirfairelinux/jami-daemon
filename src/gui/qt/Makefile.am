noinst_LTLIBRARIES = libsflphoneqt.la

BUILT_SOURCES = \
	configurationpanel.h\
	configurationpanelui.cpp\
	moc_configurationpanel.cpp\
	url_input.h \
	url_inputui.cpp \
	moc_url_input.cpp \
	moc_jpushbutton.cpp \
	moc_trayicon.cpp\
	moc_volumecontrol.cpp\
	moc_numerickeypad.cpp \
	moc_mydisplay.cpp \
	moc_qtGUImainwindow.cpp

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
	volumecontrol.cpp \
	skin.cpp skin.h 


CLEANFILES = \
	$(BUILT_SOURCES)

libsflphoneqt_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(portaudio_CFLAGS)
libsflphoneqt_la_LIBADD = $(portaudio_LIBS)

KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = -I$(top_srcdir) -I$(srcdir)/../../audio/pacpp/include/ $(QT_INCLUDES) $(X_INCLUDES) $(libccext2_CFLAGS) $(portaudio_CFLAGS)


%.h: %.ui
	$(UIC) -o $@ $<

moc_%.cpp: %.h
	$(MOC) -o $@ $<

%ui.cpp: %.ui
	$(UIC) -o $@ -impl $*.h $<


