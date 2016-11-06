include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libdevcore.la
libdevcore_la_CPPFLAGS = @CPPFLAGS@ -DBOOST_SYSTEM_NO_DEPRECATED
libdevcore_la_CXXFLAGS = @CXXFLAGS@

libdevcore_la_SOURCES = FixedHash.cpp \
	Hash.cpp \
	Guards.cpp \
	Common.cpp \
	RLP.cpp \
	SHA3.cpp \
	CommonData.cpp

noinst_HEADERS = FixedHash.h \
	Hash.h \
	Guards.h \
	Common.h \
	RLP.h \
	SHA3.h \
	CommonData.h
