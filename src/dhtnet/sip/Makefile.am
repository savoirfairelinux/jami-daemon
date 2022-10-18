noinst_LTLIBRARIES += libsiplink.la

libsiplink_la_CXXFLAGS = -I$(top_srcdir)/src $(AM_CXXFLAGS)

libsiplink_la_SOURCES = \
	./dhtnet/sip/sipaccountbase.cpp \
	./dhtnet/sip/sdp.cpp \
	./dhtnet/sip/sipaccount.cpp \
	./dhtnet/sip/sipcall.cpp \
	./dhtnet/sip/sipvoiplink.cpp \
	./dhtnet/sip/siptransport.cpp \
	./dhtnet/sip/sipaccountbase.h \
	./dhtnet/sip/sdp.h \
	./dhtnet/sip/sipaccount.h \
	./dhtnet/sip/sipcall.h \
	./dhtnet/sip/sipvoiplink.h \
	./dhtnet/sip/siptransport.h \
	./dhtnet/sip/sip_utils.cpp \
	./dhtnet/sip/sip_utils.h \
	./dhtnet/sip/sippresence.cpp \
	./dhtnet/sip/sippresence.h \
	./dhtnet/sip/pres_sub_server.cpp\
	./dhtnet/sip/pres_sub_server.h\
	./dhtnet/sip/pres_sub_client.cpp\
	./dhtnet/sip/pres_sub_client.h \
	./dhtnet/sip/sdes_negotiator.cpp \
	./dhtnet/sip/sdes_negotiator.h

libjami_la_LIBADD += libsiplink.la
