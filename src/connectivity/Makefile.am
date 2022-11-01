noinst_LTLIBRARIES += libconnectivity.la

include ./connectivity/security/Makefile.am
include ./connectivity/upnp/Makefile.am

libconnectivity_la_SOURCES = \
		./connectivity/connectionmanager.cpp \
		./connectivity/connectionmanager.h \
		./connectivity/ice_socket.h \
		./connectivity/generic_io.h \
		./connectivity/ice_transport.cpp \
		./connectivity/ice_transport.h \
		./connectivity/ip_utils.cpp \
		./connectivity/ip_utils.h \
		./connectivity/multiplexed_socket.cpp \
		./connectivity/multiplexed_socket.h \
		./connectivity/peer_connection.cpp \
		./connectivity/peer_connection.h \
		./connectivity/sip_utils.cpp \
		./connectivity/sip_utils.h \
		./connectivity/turn_transport.cpp \
		./connectivity/turn_transport.h \
		./connectivity/turn_cache.cpp \
		./connectivity/turn_cache.h \
		./connectivity/utf8_utils.cpp \
		./connectivity/utf8_utils.h \
		./connectivity/transport/peer_channel.h

libjami_la_LIBADD += libconnectivity.la
