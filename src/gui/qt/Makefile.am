noinst_LTLIBRARIES = libsflphoneqt.la


sflphone_UI = \
	configurationpanel.h\
	configurationpanelui.cpp\
	configurationpanelmoc.cpp\
	url_input.h \
	url_inputui.cpp \
	url_inputmoc.cpp

BUILT_SOURCES = \
	$(sflphone_UI) \
	trayiconmoc.cpp\
	volumecontrolmoc.cpp\
	jpushbuttonmoc.cpp \
	numerickeypadmoc.cpp \
	qtGUImainwindowmoc.cpp

libsflphoneqt_la_SOURCES = \
	$(sflphone_UI)\
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

libsflphoneqt_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\"


AM_LDFLAGS = $(KDE_RPATH) 

KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = -I$(qt_includes) $(all_includes)


%.h: %.ui
	$(UIC) -o $@ $<

%moc.cpp: %.h
	$(MOC) -o $@ $<

%ui.cpp: %.ui
	$(UIC) -o $@ -impl $*.h $<


