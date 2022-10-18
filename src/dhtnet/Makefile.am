noinst_LTLIBRARIES += libdhtnet.la

include ./dhtnet/security/Makefile.am
include ./dhtnet/upnp/Makefile.am

libdhtnet_la_SOURCES = \
		./dhtnet/connectionmanager.cpp \
		./dhtnet/connectionmanager.h \
		./dhtnet/ice_socket.h \
		./dhtnet/generic_io.h \
		./dhtnet/ice_transport.cpp \
		./dhtnet/ice_transport.h \
		./dhtnet/ip_utils.cpp \
		./dhtnet/ip_utils.h \
		./dhtnet/multiplexed_socket.cpp \
		./dhtnet/multiplexed_socket.h \
		./dhtnet/peer_connection.cpp \
		./dhtnet/peer_connection.h \
		./dhtnet/sip_utils.cpp \
		./dhtnet/sip_utils.h \
		./dhtnet/utf8_utils.cpp \
		./dhtnet/utf8_utils.h \
		./dhtnet/transport/peer_channel.h

libjami_la_LIBADD += libdhtnet.la
