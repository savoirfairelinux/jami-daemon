#################################################
# Unit tests
#################################################
ut_includedirs = ['unitTest', '../src', libjami_includedirs]
ut_dependencies = [depjami, depcppunit, libjami_dependencies]
ut_commonfiles = files('unitTest/common.cpp')


ut_account_archive = executable('ut_account_archive',
    sources: ut_commonfiles + files(
        'unitTest/account_archive/account_archive.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('account_archive', ut_account_archive, is_parallel: false, timeout: 60)


ut_account_factory = executable('ut_account_factory',
    sources: ut_commonfiles + files(
        'unitTest/account_factory/testAccount_factory.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('account_factory', ut_account_factory, is_parallel: false, timeout: 60)


ut_audio_frame_resizer = executable('ut_audio_frame_resizer',
    sources: ut_commonfiles + files(
        'unitTest/media/audio/test_audio_frame_resizer.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('audio_frame_resizer', ut_audio_frame_resizer, is_parallel: false, timeout: 60)


ut_auto_answer = executable('ut_auto_answer',
    sources: ut_commonfiles + files(
        'unitTest/media_negotiation/auto_answer.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('auto_answer', ut_auto_answer, is_parallel: false, timeout: 60)


ut_base64 = executable('ut_base64',
    sources: ut_commonfiles + files(
        'unitTest/base64/base64.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('base64', ut_base64, is_parallel: false, timeout: 60)


ut_call = executable('ut_call',
    sources: ut_commonfiles + files(
        'unitTest/call/call.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('call', ut_call, is_parallel: false, timeout: 60)


ut_certificate_store = executable('ut_certificate_store',
    sources: ut_commonfiles + files(
        'unitTest/certstore.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('certificate_store', ut_certificate_store, is_parallel: false, timeout: 60)


ut_compatibility = executable('ut_compatibility',
    sources: ut_commonfiles + files(
        'unitTest/conversation/compability.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('compatibility', ut_compatibility, is_parallel: false, timeout: 60)


ut_conference = executable('ut_conference',
    sources: ut_commonfiles + files(
        'unitTest/call/conference.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('conference', ut_conference, is_parallel: false, timeout: 60)


ut_connection_manager = executable('ut_connection_manager',
    sources: ut_commonfiles + files(
        'unitTest/connectionManager/connectionManager.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('connection_manager', ut_connection_manager, is_parallel: false, timeout: 60)


ut_conversation = executable('ut_conversation',
    sources: ut_commonfiles + files(
        'unitTest/conversation/conversationcommon.cpp',
        'unitTest/conversation/conversation.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('conversation', ut_conversation, is_parallel: false, timeout: 60)


ut_conversation_members_event = executable('ut_conversation_members_event',
    sources: ut_commonfiles + files(
        'unitTest/conversation/conversationMembersEvent.cpp',
        'unitTest/conversation/conversationcommon.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('conversation_members_event', ut_conversation_members_event, is_parallel: false, timeout: 60)


ut_conversation_repository = executable('ut_conversation_repository',
    sources: ut_commonfiles + files(
        'unitTest/conversationRepository/conversationRepository.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('conversation_repository', ut_conversation_repository, is_parallel: false, timeout: 60)


ut_conversation_request = executable('ut_conversation_request',
    sources: ut_commonfiles + files(
        'unitTest/conversation/conversationRequest.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('conversation_request', ut_conversation_request, is_parallel: false, timeout: 60)


ut_file_transfer = executable('ut_file_transfer',
    sources: ut_commonfiles + files(
        'unitTest/fileTransfer/fileTransfer.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('file_transfer', ut_file_transfer, is_parallel: false, timeout: 60)


ut_file_utils = executable('ut_file_utils',
    sources: ut_commonfiles + files(
        'unitTest/fileutils/testFileutils.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('file_utils', ut_file_utils, is_parallel: false, timeout: 60)


ut_hold_resume = executable('ut_hold_resume',
    sources: ut_commonfiles + files(
        'unitTest/media_negotiation/hold_resume.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('hold_resume', ut_hold_resume, is_parallel: false, timeout: 60)


ut_ice = executable('ut_ice',
    sources: ut_commonfiles + files(
        'unitTest/ice/ice.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('ice', ut_ice, is_parallel: false, timeout: 60)


ut_ice_media_candidates_exchange = executable('ut_ice_media_candidates_exchange',
    sources: ut_commonfiles + files(
        'unitTest/ice/ice_media_cand_exchange.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('ice_media_candidates_exchange', ut_ice_media_candidates_exchange, is_parallel: false, timeout: 60)


ut_ice_sdp_parser = executable('ut_ice_sdp_parser',
    sources: ut_commonfiles + files(
        'unitTest/ice/ice_sdp_parser.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('ice_sdp_parser', ut_ice_sdp_parser, is_parallel: false, timeout: 60)


ut_map_utils = executable('ut_map_utils',
    sources: ut_commonfiles + files(
        'unitTest/map_utils/testMap_utils.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('map_utils', ut_map_utils, is_parallel: false, timeout: 60)


ut_media_decoder = executable('ut_media_decoder',
    sources: ut_commonfiles + files(
        'unitTest/media/test_media_decoder.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('media_decoder', ut_media_decoder, is_parallel: false, timeout: 60)


ut_media_encoder = executable('ut_media_encoder',
    sources: ut_commonfiles + files(
        'unitTest/media/test_media_encoder.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('media_encoder', ut_media_encoder, is_parallel: false, timeout: 60)


ut_media_filter = executable('ut_media_filter',
    sources: ut_commonfiles + files(
        'unitTest/media/test_media_filter.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('media_filter', ut_media_filter, is_parallel: false, timeout: 60)


ut_media_frame = executable('ut_media_frame',
    sources: ut_commonfiles + files(
        'unitTest/media/test_media_frame.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('media_frame', ut_media_frame, is_parallel: false, timeout: 60)


ut_media_negotiation = executable('ut_media_negotiation',
    sources: ut_commonfiles + files(
        'unitTest/media_negotiation/media_negotiation.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('media_negotiation', ut_media_negotiation, is_parallel: false, timeout: 60)


ut_resampler = executable('ut_resampler',
    sources: ut_commonfiles + files(
        'unitTest/media/audio/test_resampler.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('resampler', ut_resampler, is_parallel: false, timeout: 60)


ut_revoke = executable('ut_revoke',
    sources: ut_commonfiles + files(
        'unitTest/revoke/revoke.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('revoke', ut_revoke, is_parallel: false, timeout: 60)


ut_scheduler = executable('ut_scheduler',
    sources: ut_commonfiles + files(
        'unitTest/scheduler.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('scheduler', ut_scheduler, is_parallel: false, timeout: 60)


ut_sip_basic_calls = executable('ut_sip_basic_calls',
    sources: ut_commonfiles + files(
        'unitTest/sip_account/sip_basic_calls.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('sip_basic_calls', ut_sip_basic_calls, is_parallel: false, timeout: 60)


#ut_sip_empty_offer = executable('ut_sip_empty_offer',
#    sources: ut_commonfiles + files(
#        'unitTest/sip_account/sip_empty_offer.cpp'
#    ),
#    include_directories: ut_includedirs,
#    dependencies: ut_dependencies
#)
#test('sip_empty_offer', ut_sip_empty_offer, is_parallel: false, timeout: 60)


ut_sip_srtp = executable('ut_sip_srtp',
    sources: ut_commonfiles + files(
        'unitTest/sip_account/sip_srtp.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('sip_srtp', ut_sip_srtp, is_parallel: false, timeout: 60)


ut_smart_tools = executable('ut_smart_tools',
    sources: ut_commonfiles + files(
        'unitTest/smartools/testSmartools.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('smart_tools', ut_smart_tools, is_parallel: false, timeout: 60)


ut_string_utils = executable('ut_string_utils',
    sources: ut_commonfiles + files(
        'unitTest/string_utils/testString_utils.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('string_utils', ut_string_utils, is_parallel: false, timeout: 60)


ut_sync_history = executable('ut_sync_history',
    sources: ut_commonfiles + files(
        'unitTest/syncHistory/syncHistory.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('sync_history', ut_sync_history, is_parallel: false, timeout: 60)


ut_utf8_utils = executable('ut_utf8_utils',
    sources: ut_commonfiles + files(
        'unitTest/utf8_utils/testUtf8_utils.cpp'
    ),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)
test('utf8_utils', ut_utf8_utils, is_parallel: false, timeout: 60)


if conf.get('ENABLE_VIDEO')
    ut_video_input = executable('ut_video_input',
        sources: ut_commonfiles + files(
            'unitTest/media/video/testVideo_input.cpp'
        ),
        include_directories: ut_includedirs,
        dependencies: ut_dependencies
    )
    test('video_input', ut_video_input, is_parallel: false, timeout: 60)


    ut_video_scaler = executable('ut_video_scaler',
        sources: ut_commonfiles + files(
            'unitTest/media/video/test_video_scaler.cpp'
        ),
        include_directories: ut_includedirs,
        dependencies: ut_dependencies
    )
    test('video_scaler', ut_video_scaler, is_parallel: false, timeout: 60)
endif
