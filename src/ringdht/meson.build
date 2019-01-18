
libringdht_includedirs = include_directories(
    'eth'
)

libringdht_sources = [
    'accountarchive.cpp',
    'eth/libdevcore/Common.cpp',
    'eth/libdevcore/CommonData.cpp',
    'eth/libdevcore/FixedHash.cpp',
    'eth/libdevcore/SHA3.cpp',
    'eth/libdevcrypto/Common.cpp',
    'p2p.cpp',
    'ringaccount.cpp',
    'sips_transport_ice.cpp'
]

if get_option('name_service')
    libringdht_sources += [
        'namedirectory.cpp'
    ]
endif

libringdht = static_library('ringdht',
    sources: libringdht_sources,
    include_directories: libringdht_includedirs
)
