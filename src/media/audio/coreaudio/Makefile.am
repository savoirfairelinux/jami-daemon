noinst_LTLIBRARIES += libcoreaudiolayer.la

libcoreaudiolayer_la_SOURCES=

if HAVE_OSX
libcoreaudiolayer_la_SOURCES += \
	./media/audio/coreaudio/osx/corelayer.mm \
	./media/audio/coreaudio/osx/corelayer.h \
	./media/audio/coreaudio/osx/audiodevice.cpp \
	./media/audio/coreaudio/osx/audiodevice.h
endif

if HAVE_IOS
libcoreaudiolayer_la_SOURCES += \
	./media/audio/coreaudio/ios/corelayer.mm \
	./media/audio/coreaudio/ios/corelayer.h
endif

libcoreaudiolayer_la_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(AM_CXXFLAGS)

libaudio_la_LIBADD += libcoreaudiolayer.la
