SUBDIRS=im

include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libsiplink.la

libsiplink_la_SOURCES = \
		Pattern.cpp \
		SdesNegotiator.cpp \
		sdp.cpp \
		sdpmedia.cpp \
		sipaccount.cpp \
		sipcall.cpp \
		sipvoiplink.cpp

libsiplink_la_LIBADD =	im/libim.la 

noinst_HEADERS = \
		Pattern.h \
		SdesNegotiator.h \
		sdp.h \
		sdpmedia.h \
		sipaccount.h \
		sipcall.h \
		sipvoiplink.h

libsiplink_la_CXXFLAGS = \
		@PCRE_LIBS@
