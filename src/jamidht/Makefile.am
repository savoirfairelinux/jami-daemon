noinst_LTLIBRARIES += libjamiacc.la

libjamiacc_la_LIBADD=

AM_CPPFLAGS += -I$(top_srcdir)/src/jamidht/eth

libjamiacc_la_SOURCES = \
	./jamidht/abstract_sip_transport.h \
	./jamidht/jamiaccount.cpp \
	./jamidht/jamiaccount.h \
	./jamidht/jamiaccount_config.cpp \
	./jamidht/jamiaccount_config.h \
	./jamidht/channeled_transport.h \
	./jamidht/channeled_transport.cpp \
	./jamidht/conversation.h \
	./jamidht/conversation.cpp \
	./jamidht/conversationrepository.h \
	./jamidht/conversationrepository.cpp \
	./jamidht/gitserver.h \
	./jamidht/gitserver.cpp \
	./jamidht/channel_handler.h \
	./jamidht/conversation_channel_handler.h \
	./jamidht/conversation_channel_handler.cpp \
	./jamidht/conversation_module.h \
	./jamidht/conversation_module.cpp \
	./jamidht/accountarchive.cpp \
	./jamidht/accountarchive.h \
	./jamidht/jami_contact.h \
	./jamidht/contact_list.h \
	./jamidht/contact_list.cpp \
	./jamidht/account_manager.h \
	./jamidht/account_manager.cpp \
	./jamidht/archive_account_manager.h \
	./jamidht/archive_account_manager.cpp \
	./jamidht/server_account_manager.h \
	./jamidht/server_account_manager.cpp \
	./jamidht/sync_channel_handler.h \
	./jamidht/sync_channel_handler.cpp \
	./jamidht/sync_module.h \
	./jamidht/sync_module.cpp \
	./jamidht/transfer_channel_handler.h \
	./jamidht/transfer_channel_handler.cpp \
	./jamidht/swarm/routing_table.h \
	./jamidht/swarm/routing_table.cpp \
	./jamidht/swarm/swarm_protocol.h \
	./jamidht/swarm/swarm_protocol.cpp \
	./jamidht/swarm/swarm_channel_handler.h \
	./jamidht/swarm/swarm_channel_handler.cpp \
	./jamidht/swarm/swarm_manager.h \
	./jamidht/swarm/swarm_manager.cpp


if RINGNS
libjamiacc_la_SOURCES += \
	./jamidht/namedirectory.cpp \
	./jamidht/namedirectory.h
endif

include ./jamidht/eth/libdevcore/Makefile.am
include ./jamidht/eth/libdevcrypto/Makefile.am

libjami_la_LIBADD += libjamiacc.la
