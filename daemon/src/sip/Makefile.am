include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libsiplink.la
libsiplink_la_CXXFLAGS = @CXXFLAGS@

libsiplink_la_SOURCES = \
        sdp.cpp \
        sipaccount.cpp \
        sipcall.cpp \
        sipvoiplink.cpp \
        siptransport.cpp \
        sdp.h \
        sipaccount.h \
        sipcall.h \
        sipvoiplink.h \
        siptransport.h \
        sip_utils.cpp \
        sip_utils.h

if BUILD_TLS
libsiplink_la_SOURCES += tlsvalidation.c \
                         tlsvalidation.h
endif

if SFL_PRESENCE
libsiplink_la_SOURCES+=sippresence.cpp \
                       sippresence.h \
                       pres_sub_server.cpp\
                       pres_sub_server.h\
                       pres_sub_client.cpp\
                       pres_sub_client.h
endif


if BUILD_SDES
libsiplink_la_SOURCES+= sdes_negotiator.cpp \
            sdes_negotiator.h \
            pattern.cpp \
            pattern.h


libsiplink_la_CXXFLAGS += @PCRE_CFLAGS@
endif
