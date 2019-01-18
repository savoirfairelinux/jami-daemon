libaudio_includedirs = include_directories(
    'alsa',
    'coreaudio',
    'coreaudio/ios',
    'coreaudio/osx',
    'jack',
    'opensl',
    'portaudio',
    'pulseaudio',
    'sound'
)

libaudio_sources = [
    'audiobuffer.cpp',
    'audio_frame_resizer.cpp',
    'audio_input.cpp',
    'audiolayer.cpp',
    'audioloop.cpp',
    'audio_receive_thread.cpp',
    'audio_rtp_session.cpp',
    'audio_sender.cpp',
    'dcblocker.cpp',
    'resampler.cpp',
    'ringbuffer.cpp',
    'ringbufferpool.cpp',
    'sound/audiofile.cpp',
    'sound/dtmf.cpp',
    'sound/dtmfgenerator.cpp',
    'sound/tone.cpp',
    'sound/tonelist.cpp',
    'tonecontrol.cpp'
]

if host_machine.system() == 'darwin'
    if host_machine.cpu_family() == 'x86_64'
        libaudio_sources += [
            'coreaudio/osx/audiodevice.cpp',
            'coreaudio/osx/corelayer.cpp'
        ]
    else
        libaudio_sources += [
            'coreaudio/ios/corelayer.cpp'
        ]
    endif
endif

if depalsa.found()
    libaudio_sources += [
        'alsa/alsalayer.cpp'
    ]
endif

if deppulseaudio.found()
    libaudio_sources += [
        'pulseaudio/audiostream.cpp',
        'pulseaudio/pulselayer.cpp'
    ]
endif

if depjack.found()
   libaudio_sources += [
        'jack/jacklayer.cpp'
   ]
endif

if depopensl.found()
    libaudio_sources += [
        'opensl/audio_player.cpp',
        'opensl/audio_recorder.cpp',
        'opensl/opensllayer.cpp'
    ]
endif

if depportaudio.found()
    libaudio_sources += [
        'portaudio/portaudiolayer.cpp'
    ]
endif

libaudio = static_library('audio',
    sources: libaudio_sources,
    include_directories: libaudio_includedirs
)
