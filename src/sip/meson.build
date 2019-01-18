
libsip_includedirs = include_directories(
    '..'
)

libsip_sources = [
    'pres_sub_client.cpp',
    'pres_sub_server.cpp',
    'sdp.cpp',
    'sipaccountbase.cpp',
    'sipaccount.cpp',
    'sipcall.cpp',
    'sippresence.cpp',
    'siptransport.cpp',
    'sip_utils.cpp',
    'sipvoiplink.cpp'
]

if conf.get('HAVE_SDES')
    libsip_sources += [
        'pattern.cpp',
        'sdes_negotiator.cpp'
    ]
endif

libsip = static_library('sip',
    sources: libsip_sources,
    include_directories: libsip_includedirs
)
