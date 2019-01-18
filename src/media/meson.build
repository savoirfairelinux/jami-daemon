subdir('audio')

libmedia_libs = [libaudio]

libmedia_sources = [
    'libav_utils.cpp',
    'localrecorder.cpp',
    'localrecordermanager.cpp',
    'media_buffer.cpp',
    'media_codec.cpp',
    'media_decoder.cpp',
    'media_encoder.cpp',
    'media_filter.cpp',
    'media_io_handle.cpp',
    'media_recorder.cpp',
    'recordable.cpp',
    'socket_pair.cpp',
    'system_codec_container.cpp'
]


if get_option('video')
    subdir('video')
    libmedia_libs += [libvideo]
endif

libmedia = static_library('media',
    sources: libmedia_sources,
    link_with: libmedia_libs
)
