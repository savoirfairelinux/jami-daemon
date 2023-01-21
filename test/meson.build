#################################################
# Unit tests
#################################################
ut_includedirs = ['unitTest', '../src', libjami_includedirs]
ut_dependencies = [depjami, depcppunit, libjami_dependencies]
ut_workdir = meson.project_source_root() / 'test' / 'unitTest'
ut_library = static_library('common',
    sources: files('unitTest/common.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies
)


ut_account_archive = executable('ut_account_archive',
    sources: files('unitTest/account_archive/account_archive.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('account_archive', ut_account_archive,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_account_factory = executable('ut_account_factory',
    sources: files('unitTest/account_factory/testAccount_factory.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('account_factory', ut_account_factory,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_audio_frame_resizer = executable('ut_audio_frame_resizer',
    sources: files('unitTest/media/audio/test_audio_frame_resizer.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('audio_frame_resizer', ut_audio_frame_resizer,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_auto_answer = executable('ut_auto_answer',
    sources: files('unitTest/media_negotiation/auto_answer.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('auto_answer', ut_auto_answer,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_base64 = executable('ut_base64',
    sources: files('unitTest/base64/base64.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('base64', ut_base64,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_call = executable('ut_call',
    sources: files('unitTest/call/call.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('call', ut_call,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_certificate_store = executable('ut_certificate_store',
    sources: files('unitTest/certstore.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('certificate_store', ut_certificate_store,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_conference = executable('ut_conference',
    sources: files('unitTest/call/conference.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('conference', ut_conference,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_connection_manager = executable('ut_connection_manager',
    sources: files('unitTest/connectionManager/connectionManager.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('connection_manager', ut_connection_manager,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_conversation = executable('ut_conversation',
    sources: files(
        'unitTest/conversation/conversationcommon.cpp',
        'unitTest/conversation/conversation.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('conversation', ut_conversation,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_conversation_call = executable('ut_conversation_call',
    sources: files(
        'unitTest/conversation/conversationcommon.cpp',
        'unitTest/conversation/call.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('conversation_call', ut_conversation_call,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_conversation_members_event = executable('ut_conversation_members_event',
    sources: files(
        'unitTest/conversation/conversationMembersEvent.cpp',
        'unitTest/conversation/conversationcommon.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('conversation_members_event', ut_conversation_members_event,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_conversation_repository = executable('ut_conversation_repository',
    sources: files('unitTest/conversationRepository/conversationRepository.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('conversation_repository', ut_conversation_repository,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_conversation_request = executable('ut_conversation_request',
    sources: files('unitTest/conversation/conversationRequest.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('conversation_request', ut_conversation_request,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_file_transfer = executable('ut_file_transfer',
    sources: files('unitTest/fileTransfer/fileTransfer.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('file_transfer', ut_file_transfer,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_file_utils = executable('ut_file_utils',
    sources: files('unitTest/fileutils/testFileutils.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('file_utils', ut_file_utils,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_hold_resume = executable('ut_hold_resume',
    sources: files('unitTest/media_negotiation/hold_resume.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('hold_resume', ut_hold_resume,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_ice = executable('ut_ice',
    sources: files('unitTest/ice/ice.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('ice', ut_ice,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_ice_media_candidates_exchange = executable('ut_ice_media_candidates_exchange',
    sources: files('unitTest/ice/ice_media_cand_exchange.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('ice_media_candidates_exchange', ut_ice_media_candidates_exchange,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_ice_sdp_parser = executable('ut_ice_sdp_parser',
    sources: files('unitTest/ice/ice_sdp_parser.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('ice_sdp_parser', ut_ice_sdp_parser,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_map_utils = executable('ut_map_utils',
    sources: files('unitTest/map_utils/testMap_utils.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('map_utils', ut_map_utils,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_media_decoder = executable('ut_media_decoder',
    sources: files('unitTest/media/test_media_decoder.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('media_decoder', ut_media_decoder,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_media_encoder = executable('ut_media_encoder',
    sources: files('unitTest/media/test_media_encoder.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('media_encoder', ut_media_encoder,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_media_filter = executable('ut_media_filter',
    sources: files('unitTest/media/test_media_filter.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library,
)
test('media_filter', ut_media_filter,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_media_frame = executable('ut_media_frame',
    sources: files('unitTest/media/test_media_frame.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('media_frame', ut_media_frame,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_media_player = executable('ut_media_player',
    sources: files('unitTest/media/test_media_player.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('media_player', ut_media_player,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_media_negotiation = executable('ut_media_negotiation',
    sources: files('unitTest/media_negotiation/media_negotiation.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('media_negotiation', ut_media_negotiation,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_migration = executable('ut_migration',
    sources: files('unitTest/account_archive/migration.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('migration', ut_migration,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_recorder = executable('ut_recorder',
    sources: files('unitTest/call/recorder.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('recorder', ut_recorder,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_resampler = executable('ut_resampler',
    sources: files('unitTest/media/audio/test_resampler.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('resampler', ut_resampler,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_revoke = executable('ut_revoke',
    sources: files('unitTest/revoke/revoke.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('revoke', ut_revoke,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_scheduler = executable('ut_scheduler',
    sources: files('unitTest/scheduler.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('scheduler', ut_scheduler,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_sip_basic_calls = executable('ut_sip_basic_calls',
    sources: files('unitTest/sip_account/sip_basic_calls.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('sip_basic_calls', ut_sip_basic_calls,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_sip_srtp = executable('ut_sip_srtp',
    sources: files('unitTest/sip_account/sip_srtp.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('sip_srtp', ut_sip_srtp,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_string_utils = executable('ut_string_utils',
    sources: files('unitTest/string_utils/testString_utils.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('string_utils', ut_string_utils,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_sync_history = executable('ut_sync_history',
    sources: files('unitTest/syncHistory/syncHistory.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('sync_history', ut_sync_history,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


ut_utf8_utils = executable('ut_utf8_utils',
    sources: files('unitTest/utf8_utils/testUtf8_utils.cpp'),
    include_directories: ut_includedirs,
    dependencies: ut_dependencies,
    link_with: ut_library
)
test('utf8_utils', ut_utf8_utils,
    workdir: ut_workdir, is_parallel: false, timeout: 1800
)


if conf.get('ENABLE_VIDEO')
    ut_plugins = executable('ut_plugins',
        sources: files('unitTest/plugins/plugins.cpp'),
        include_directories: ut_includedirs,
        dependencies: ut_dependencies,
        link_with: ut_library
    )
    test('plugins', ut_plugins,
        workdir: ut_workdir, is_parallel: false, timeout: 1800
    )


    ut_video_input = executable('ut_video_input',
        sources: files('unitTest/media/video/testVideo_input.cpp'),
        include_directories: ut_includedirs,
        dependencies: ut_dependencies,
        link_with: ut_library
    )
    test('video_input', ut_video_input,
        workdir: ut_workdir, is_parallel: false, timeout: 1800
    )


    ut_video_scaler = executable('ut_video_scaler',
        sources: files('unitTest/media/video/test_video_scaler.cpp'),
        include_directories: ut_includedirs,
        dependencies: ut_dependencies,
        link_with: ut_library
    )
    test('video_scaler', ut_video_scaler,
        workdir: ut_workdir, is_parallel: false, timeout: 1800
    )
endif
