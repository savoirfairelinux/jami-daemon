progsipp = find_program('sipp', native: true, required: false)
if progsipp.found()
    ut_sip = executable('ut_sip',
        include_directories: include_directories('..', '../src', '../src/jamid', '../src/media'),
        sources: files('sip/sip.cpp', 'sip/test_SIP.cpp'),
        link_with: libjami,
        dependencies: [depcppunit, deplibpjproject]
    )
    test('sip', ut_sip, workdir: meson.current_source_dir() / 'sip')
endif

#ut_turn = executable('ut_turn',
#    include_directories: include_directories('../src', '../src/jamid'),
#    sources: files('turn/main.cpp', 'turn/test_TURN.cpp'),
#    link_with: libjami,
#    dependencies: [depcppunit, depopendht]
#)
#test('turn', ut_turn)


####### Unit Tests ########

#
# account_factory
#
ut_account_factory = executable('ut_account_factory',
    include_directories: include_directories('../src', '../src/jamid'),
    sources: files('unitTest/account_factory/testAccount_factory.cpp'),
    link_with: libjami,
    dependencies: depcppunit
)
test('account_factory', ut_account_factory)

#
# certstore
#
ut_certstore = executable('ut_certstore',
    include_directories: include_directories('../src', '../src/jamid'),
    sources: files('unitTest/certstore.cpp'),
    link_with: libjami,
    dependencies: [depcppunit, depopendht]
)
test('certstore', ut_certstore)

#
# base64
#
ut_base64 = executable('ut_base64',
    include_directories: include_directories('../src', '../src/jamid'),
    sources: files('unitTest/base64/base64.cpp'),
    link_with: libjami,
    dependencies: depcppunit
)
test('base64', ut_base64)

#
# channel
#
ut_channel = executable('ut_channel',
    include_directories: include_directories('../src', '../src/jamid'),
    sources: files('unitTest/channel/testChannel.cpp'),
    link_with: libjami,
    dependencies: [depcppunit, depthreads]
)
test('channel', ut_channel)

#
# map_utils
#
ut_map_utils = executable('ut_map_utils',
    include_directories: include_directories('../src', '../src/jamid'),
    sources: files('unitTest/map_utils/testMap_utils.cpp'),
    link_with: libjami,
    dependencies: depcppunit
)
test('map_utils', ut_map_utils)

#
# fileutils
#
ut_fileutils = executable('ut_fileutils',
    include_directories: include_directories('../src', '../src/jamid'),
    sources: files('unitTest/fileutils/testFileutils.cpp'),
    link_with: libjami,
    dependencies: depcppunit
)
test('fileutils', ut_fileutils)

#
# smartools
#
ut_smartools = executable('ut_smartools',
    include_directories: include_directories('../src', '../src/jamid'),
    sources: files('unitTest/smartools/testSmartools.cpp'),
    link_with: libjami,
    dependencies: depcppunit
)
test('smartools', ut_smartools)

#
# utf8_utils
#
ut_utf8_utils = executable('ut_utf8_utils',
    include_directories: include_directories('../src', '../src/jamid'),
    sources: files('unitTest/utf8_utils/testUtf8_utils.cpp'),
    link_with: libjami,
    dependencies: depcppunit
)
test('utf8_utils', ut_utf8_utils)

#
# string_utils
#
ut_string_utils = executable('ut_string_utils',
    include_directories: include_directories('../src', '../src/jamid'),
    sources: files('unitTest/string_utils/testString_utils.cpp'),
    link_with: libjami,
    dependencies: depcppunit
)
test('string_utils', ut_string_utils)

#
# media_encoder
#
ut_media_encoder = executable('ut_media_encoder',
    include_directories: include_directories('..', '../src', '../src/jamid', '../src/media'),
    sources: files('unitTest/media/test_media_encoder.cpp'),
    link_with: libjami,
    dependencies: [depcppunit, deplibpjproject, deplibavutil]
)
test('media_encoder', ut_media_encoder)

#
# media_decoder
#
ut_media_decoder = executable('ut_media_decoder',
    include_directories: include_directories('..', '../src', '../src/jamid', '../src/media'),
    sources: files('unitTest/media/test_media_decoder.cpp'),
    link_with: libjami,
    dependencies: [depcppunit, deplibpjproject, deplibavcodec]
)
test('media_decoder', ut_media_decoder)

#
# media_filter
#
#ut_media_filter = executable('ut_media_filter',
#    include_directories: include_directories('..', '../src', '../src/jamid', '../src/media'),
#    sources: files('unitTest/media/test_media_filter.cpp'),
#    link_with: libjami,
#    dependencies: depcppunit
#)
#test('media_filter', ut_media_filter)

#
# media_frame
#
ut_media_frame = executable('ut_media_frame',
    include_directories: include_directories('..', '../src', '../src/jamid', '../src/media'),
    sources: files('unitTest/media/test_media_frame.cpp'),
    link_with: libjami,
    dependencies: depcppunit
)
test('media_frame', ut_media_frame)

#
# resampler
#
ut_resampler = executable('ut_resampler',
    include_directories: include_directories('..', '../src', '../src/jamid', '../src/media'),
    sources: files('unitTest/media/audio/test_resampler.cpp'),
    link_with: libjami,
    dependencies: depcppunit
)
test('resampler', ut_resampler)

#
# audio_frame_resizer
#
ut_audio_frame_resizer = executable('ut_audio_frame_resizer',
    include_directories: include_directories('..', '../src', '../src/jamid', '../src/media'),
    sources: files('unitTest/media/audio/test_audio_frame_resizer.cpp'),
    link_with: libjami,
    dependencies: [depcppunit, deplibavutil]
)
test('audio_frame_resizer', ut_audio_frame_resizer)

if conf.get('ENABLE_VIDEO')
    #
    # video_input
    #
    ut_video_input = executable('ut_video_input',
        include_directories: include_directories('..', '../src', '../src/jamid', '../src/media'),
        sources: files('unitTest/media/video/testVideo_input.cpp'),
        link_with: libjami,
        dependencies: depcppunit
    )
    test('video_input', ut_video_input)

    #
    # video_scaler
    #
    ut_video_scaler = executable('ut_video_scaler',
        include_directories: include_directories('../src', '../src/jamid'),
        sources: files('unitTest/media/video/test_video_scaler.cpp'),
        link_with: libjami,
        dependencies: depcppunit
    )
    test('video_scaler', ut_video_scaler)
endif

#
# connectionManager
#
ut_connectionManager = executable('ut_connectionManager',
    include_directories: include_directories('..', '../src', '../src/jamid', '../src/media'),
    sources: files('unitTest/connectionManager/connectionManager.cpp'),
    link_with: libjami,
    dependencies: [depcppunit, deplibpjproject]
)
test('connectionManager', ut_connectionManager)
