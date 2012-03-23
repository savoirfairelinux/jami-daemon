include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libsiplink.la

libsiplink_la_SOURCES = \
		pattern.cpp \
		sdes_negotiator.cpp \
		sdp.cpp \
		sipaccount.cpp \
		sipcall.cpp \
		sipvoiplink.cpp \
		pattern.h \
		sdes_negotiator.h \
		sdp.h \
		sipaccount.h \
		sipcall.h \
		sipvoiplink.h \
		sip_utils.cpp \
		sip_utils.h

libsiplink_la_CXXFLAGS = \
		@PCRE_LIBS@
