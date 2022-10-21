noinst_LTLIBRARIES += libsiplink.la

libsiplink_la_CXXFLAGS = -I$(top_srcdir)/src $(AM_CXXFLAGS)

libsiplink_la_SOURCES = \
	./sip/sipaccountbase.cpp \
	./sip/sdp.cpp \
	./sip/sipaccount.cpp \
	./sip/sipcall.cpp \
	./sip/sipvoiplink.cpp \
	./sip/siptransport.cpp \
	./sip/sipaccountbase.h \
	./sip/sipaccountbase_config.h \
	./sip/sipaccountbase_config.cpp \
	./sip/sipaccount_config.h \
	./sip/sipaccount_config.cpp \
	./sip/sdp.h \
	./sip/sipaccount.h \
	./sip/sipcall.h \
	./sip/sipvoiplink.h \
	./sip/siptransport.h \
	./sip/sip_utils.cpp \
	./sip/sip_utils.h \
	./sip/sippresence.cpp \
	./sip/sippresence.h \
	./sip/pres_sub_server.cpp\
	./sip/pres_sub_server.h\
	./sip/pres_sub_client.cpp\
	./sip/pres_sub_client.h \
	./sip/sdes_negotiator.cpp \
	./sip/sdes_negotiator.h

libjami_la_LIBADD += libsiplink.la
