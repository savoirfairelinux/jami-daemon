# Rules for the test code (use `make check` to execute)
include $(top_srcdir)/globals.mk

AM_CXXFLAGS += -I$(top_srcdir)/src
AM_LDFLAGS += $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la
check_PROGRAMS =

####### Unit Test ########

#
# account_factory
#
check_PROGRAMS += ut_account_factory
ut_account_factory_SOURCES = account_factory/testAccount_factory.cpp

#
# certstore
#
check_PROGRAMS += ut_certstore
ut_certstore_SOURCES = certstore.cpp
ut_certstore_LDADD = @OPENDHT_LIBS@

#
# scheduler
#
check_PROGRAMS += ut_scheduler
ut_scheduler_SOURCES = scheduler.cpp
ut_scheduler_LDADD = @OPENDHT_LIBS@

#
# base64
#
check_PROGRAMS += ut_base64
ut_base64_SOURCES = base64/base64.cpp

#
# channel
#
check_PROGRAMS += ut_channel
ut_channel_SOURCES = channel/testChannel.cpp

#
# map_utils
#
check_PROGRAMS += ut_map_utils
ut_map_utils_SOURCES = map_utils/testMap_utils.cpp

#
# fileutils
#
check_PROGRAMS += ut_fileutils
ut_fileutils_SOURCES = fileutils/testFileutils.cpp

#
# smartools
#
check_PROGRAMS += ut_smartools
ut_smartools_SOURCES = smartools/testSmartools.cpp

#
# utf8_utils
#
check_PROGRAMS += ut_utf8_utils
ut_utf8_utils_SOURCES = utf8_utils/testUtf8_utils.cpp

#
# string_utils
#
check_PROGRAMS += ut_string_utils
ut_string_utils_SOURCES = string_utils/testString_utils.cpp

#
# video_input
#
check_PROGRAMS += ut_video_input
ut_video_input_SOURCES = media/video/testVideo_input.cpp

#
# media_encoder
#
check_PROGRAMS += ut_media_encoder
ut_media_encoder_SOURCES = media/test_media_encoder.cpp

#
# media_decoder
#
check_PROGRAMS += ut_media_decoder
ut_media_decoder_SOURCES = media/test_media_decoder.cpp

#
# media_filter
#
check_PROGRAMS += ut_media_filter
ut_media_filter_SOURCES = media/test_media_filter.cpp

#
# resampler
#
check_PROGRAMS += ut_resampler
ut_resampler_SOURCES = media/audio/test_resampler.cpp

#
# media_frame
#
check_PROGRAMS += ut_media_frame
ut_media_frame_SOURCES = media/test_media_frame.cpp

#
# video_scaler
#
check_PROGRAMS += ut_video_scaler
ut_video_scaler_SOURCES = media/video/test_video_scaler.cpp

#
# audio_frame_resizer
#
check_PROGRAMS += ut_audio_frame_resizer
ut_audio_frame_resizer_SOURCES = media/audio/test_audio_frame_resizer.cpp

#
# call
#
check_PROGRAMS += ut_call
ut_call_SOURCES = call/call.cpp

#
# connectionManager
#
check_PROGRAMS += ut_connectionManager
ut_connectionManager_SOURCES = connectionManager/connectionManager.cpp

#
# fileTransfer
#
check_PROGRAMS += ut_fileTransfer
ut_fileTransfer_SOURCES = fileTransfer/fileTransfer.cpp

# conversationRepository
#
check_PROGRAMS += ut_conversationRepository
ut_conversationRepository_SOURCES = conversationRepository/conversationRepository.cpp

#
# conversation
#
check_PROGRAMS += ut_conversation
ut_conversation_SOURCES = conversation/conversation.cpp
#
# media_negotiation
#
check_PROGRAMS += ut_media_negotiation
ut_media_negotiation_SOURCES = media_negotiation/media_negotiation.cpp

#
# compability
#
check_PROGRAMS += ut_compability
ut_compability_SOURCES = conversation/compability.cpp

#
# syncHistory
#
check_PROGRAMS += ut_syncHistory
ut_syncHistory_SOURCES = syncHistory/syncHistory.cpp

#
# ice
#
check_PROGRAMS += ut_ice
ut_ice_SOURCES = ice/ice.cpp

TESTS = $(check_PROGRAMS)
