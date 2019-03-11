include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libsiplink.la
libsiplink_la_CXXFLAGS = @CXXFLAGS@ -I$(top_srcdir)/src

libsiplink_la_SOURCES = \
        sipaccountbase.cpp \
        sdp.cpp \
        sipaccount.cpp \
        sipcall.cpp \
        sipvoiplink.cpp \
        siptransport.cpp \
        sipaccountbase.h \
        sdp.h \
        sipaccount.h \
        sipcall.h \
        sipvoiplink.h \
        siptransport.h \
        sip_utils.cpp \
        sip_utils.h

libsiplink_la_SOURCES+=sippresence.cpp \
                       sippresence.h \
                       pres_sub_server.cpp\
                       pres_sub_server.h\
                       pres_sub_client.cpp\
                       pres_sub_client.h

libsiplink_la_SOURCES+= sdes_negotiator.cpp \
            sdes_negotiator.h
