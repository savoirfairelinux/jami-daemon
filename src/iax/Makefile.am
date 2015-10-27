include $(top_srcdir)/globals.mak

if USE_IAX

noinst_LTLIBRARIES = libiaxlink.la

libiaxlink_la_SOURCES = \
		iaxaccount.cpp \
		iaxcall.cpp \
		iaxvoiplink.cpp

libiaxlink_la_CXXFLAGS = \
		-DUSE_IAX

noinst_HEADERS = \
		iaxaccount.h \
		iaxcall.h \
		iaxvoiplink.h

endif

