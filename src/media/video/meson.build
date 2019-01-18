libvideo_includedirs = include_directories(
    'androidvideo',
    'iosvideo',
    'osxvideo',
    'uwpvideo',
    'v4l2',
    'winvideo'
)

libvideo_sources = [
    'sinkclient.cpp',
    #'uwpvideo/video_device_impl.cpp',
    #'uwpvideo/video_device_monitor_impl.cpp',
    'video_base.cpp',
    'video_device_monitor.cpp',
    'video_input.cpp',
    'video_mixer.cpp',
    'video_receive_thread.cpp',
    'video_rtp_session.cpp',
    'video_scaler.cpp',
    'video_sender.cpp'
]

if host_machine.system() == 'linux'
    if host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
        libvideo_sources += [
            'v4l2/video_device_impl.cpp',
            'v4l2/video_device_monitor_impl.cpp'
        ]
    else
        libvideo_sources += [
            'androidvideo/video_device_impl.cpp',
            'androidvideo/video_device_monitor_impl.cpp'
        ]
    endif
elif host_machine.system() == 'darwin'
    if host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
        libvideo_sources += [
            'osxvideo/video_device_impl.mm',
            'osxvideo/video_device_monitor_impl.mm'
        ]
    else
        libvideo_sources += [
            'iosvideo/video_device_impl.cpp',
            'iosvideo/video_device_monitor_impl.cpp'
        ]
    endif
elif host_machine.system() == 'windows'
        libvideo_sources += [
            'winvideo/video_device_impl.cpp',
            'winvideo/video_device_monitor_impl.cpp'
        ]
endif

libvideo = static_library('video',
    sources: libvideo_sources,
    include_directories: libvideo_includedirs
)
