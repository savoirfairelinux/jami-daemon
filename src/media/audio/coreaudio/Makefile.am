noinst_LTLIBRARIES += libcoreaudiolayer.la

if HAVE_OSX
libcoreaudiolayer_la_SOURCES = \
	./media/audio/osx/corelayer.mm \
	./media/audio/osx/corelayer.h \
	./media/audio/osx/audiodevice.cpp \
	./media/audio/osx/audiodevice.h
endif

if HAVE_IOS
libcoreaudiolayer_la_SOURCES = \
	./media/audio/ios/corelayer.mm ios/corelayer.h
endif

libcoreaudiolayer_la_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(AM_CXXFLAGS)

libcoreaudiolayer_la_OBJCXXFLAGS = -std=c++17

libaudio_la_LIBADD += libcoreaudiolayer.la
