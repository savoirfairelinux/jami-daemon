# Rules for the test code (use `make check` to execute)
include $(top_srcdir)/globals.mk

AM_TESTS_ENVIRONMENT =                    \
	. $(top_srcdir)/test/test-env.sh; \
	echo RUNNING: "$$f";

# The unit tests require hidden symbols.  Thus, we link the tests
# against a static version of libjami instead.
AM_CXXFLAGS += -I$(top_srcdir)/src
AM_LDFLAGS += $(CPPUNIT_LIBS) $(top_builddir)/src/libjami.la -static
check_PROGRAMS =
# Following line is a hack
AM_LDFLAGS += -lopendht -lupnp -lgnutls -lwebrtc_audio_processing -lsecp256k1
####### Unit Test ########

#
# account_factory
#
check_PROGRAMS += ut_account_factory
ut_account_factory_SOURCES = account_factory/testAccount_factory.cpp common.cpp

#
# account_archive
#
check_PROGRAMS += ut_account_archive
ut_account_archive_SOURCES = account_archive/account_archive.cpp common.cpp

#
# migration
#
check_PROGRAMS += ut_migration
ut_migration_SOURCES = account_archive/migration.cpp common.cpp

#
# certstore
#
check_PROGRAMS += ut_certstore
ut_certstore_SOURCES = certstore.cpp common.cpp

#
# scheduler
#
check_PROGRAMS += ut_scheduler
ut_scheduler_SOURCES = scheduler.cpp common.cpp

#
# base64
#
check_PROGRAMS += ut_base64
ut_base64_SOURCES = base64/base64.cpp common.cpp

#
# map_utils
#
check_PROGRAMS += ut_map_utils
ut_map_utils_SOURCES = map_utils/testMap_utils.cpp common.cpp

#
# fileutils
#
check_PROGRAMS += ut_fileutils
ut_fileutils_SOURCES = fileutils/testFileutils.cpp common.cpp

#
# utf8_utils
#
check_PROGRAMS += ut_utf8_utils
ut_utf8_utils_SOURCES = utf8_utils/testUtf8_utils.cpp common.cpp

#
# string_utils
#
check_PROGRAMS += ut_string_utils
ut_string_utils_SOURCES = string_utils/testString_utils.cpp common.cpp

#
# video_input
#
check_PROGRAMS += ut_video_input
ut_video_input_SOURCES = media/video/testVideo_input.cpp common.cpp

#
# media_encoder
#
check_PROGRAMS += ut_media_encoder
ut_media_encoder_SOURCES = media/test_media_encoder.cpp common.cpp

#
# media_decoder
#
check_PROGRAMS += ut_media_decoder
ut_media_decoder_SOURCES = media/test_media_decoder.cpp common.cpp

#
# media_filter
#
check_PROGRAMS += ut_media_filter
ut_media_filter_SOURCES = media/test_media_filter.cpp common.cpp

#
# media_player
#
check_PROGRAMS += ut_media_player
ut_media_player_SOURCES = media/test_media_player.cpp common.cpp

#
# resampler
#
check_PROGRAMS += ut_resampler
ut_resampler_SOURCES = media/audio/test_resampler.cpp common.cpp

#
# media_frame
#
check_PROGRAMS += ut_media_frame
ut_media_frame_SOURCES = media/test_media_frame.cpp common.cpp

#
# video_scaler
#
check_PROGRAMS += ut_video_scaler
ut_video_scaler_SOURCES = media/video/test_video_scaler.cpp common.cpp

#
# audio_frame_resizer
#
check_PROGRAMS += ut_audio_frame_resizer
ut_audio_frame_resizer_SOURCES = media/audio/test_audio_frame_resizer.cpp common.cpp

#
# call
#
check_PROGRAMS += ut_call
ut_call_SOURCES = call/call.cpp common.cpp

#
# recorder
#
check_PROGRAMS += ut_recorder
ut_recorder_SOURCES = call/recorder.cpp common.cpp

#
# conference
#
check_PROGRAMS += ut_conference
ut_conference_SOURCES = call/conference.cpp common.cpp

#
# connectionManager
#
check_PROGRAMS += ut_connectionManager
ut_connectionManager_SOURCES = connectionManager/connectionManager.cpp common.cpp

#
# fileTransfer
#
check_PROGRAMS += ut_fileTransfer
ut_fileTransfer_SOURCES = fileTransfer/fileTransfer.cpp common.cpp

# conversationRepository
#
check_PROGRAMS += ut_conversationRepository
ut_conversationRepository_SOURCES = conversationRepository/conversationRepository.cpp common.cpp

#
# conversation
#
check_PROGRAMS += ut_conversation
ut_conversation_SOURCES = conversation/conversationcommon.cpp conversation/conversation.cpp common.cpp

#
# conversation_call
#
check_PROGRAMS += ut_conversation_call
ut_conversation_call_SOURCES = conversation/conversationcommon.cpp conversation/call.cpp common.cpp

#
# media_negotiation
#
check_PROGRAMS += ut_media_negotiation
ut_media_negotiation_SOURCES = media_negotiation/media_negotiation.cpp common.cpp

check_PROGRAMS += ut_hold_resume
ut_hold_resume_SOURCES = media_negotiation/hold_resume.cpp common.cpp

check_PROGRAMS += ut_auto_answer
ut_auto_answer_SOURCES = media_negotiation/auto_answer.cpp common.cpp

#
# conversationRequest
#
check_PROGRAMS += ut_conversationRequest
ut_conversationRequest_SOURCES = conversation/conversationRequest.cpp common.cpp

#
# conversationMembersEvent
#
check_PROGRAMS += ut_conversationMembersEvent
ut_conversationMembersEvent_SOURCES = conversation/conversationMembersEvent.cpp conversation/conversationcommon.cpp common.cpp

#
# syncHistory
#
check_PROGRAMS += ut_syncHistory
ut_syncHistory_SOURCES = syncHistory/syncHistory.cpp common.cpp

#
# ice
#
check_PROGRAMS += ut_ice
ut_ice_SOURCES = ice/ice.cpp common.cpp

#
# revoke
#
check_PROGRAMS += ut_revoke
ut_revoke_SOURCES = revoke/revoke.cpp common.cpp

check_PROGRAMS += ut_ice_sdp_parser
ut_ice_sdp_parser_SOURCES = ice/ice_sdp_parser.cpp common.cpp

check_PROGRAMS += ut_ice_media_cand_exchange
ut_ice_media_cand_exchange_SOURCES = ice/ice_media_cand_exchange.cpp common.cpp

#
# Calls using SIP accounts
#
check_PROGRAMS += ut_sip_basic_calls
ut_sip_basic_calls_SOURCES = sip_account/sip_basic_calls.cpp

check_PROGRAMS += ut_sip_srtp
ut_sip_srtp_SOURCES = sip_account/sip_srtp.cpp

#
# Plugins
#
check_PROGRAMS += ut_plugins
ut_plugins_SOURCES = plugins/plugins.cpp common.cpp

#
# Routing Table
#
check_PROGRAMS += ut_routing_table
ut_routing_table_SOURCES = swarm/routing_table.cpp

#
# Bootstrap
#
check_PROGRAMS += ut_bootstrap
ut_bootstrap_SOURCES = swarm/bootstrap.cpp common.cpp

#
# SwarmConversation
#
check_PROGRAMS += ut_swarm_conversation
ut_swarm_conversation_SOURCES = swarm/swarm_conversation.cpp common.cpp

TESTS = $(check_PROGRAMS)
