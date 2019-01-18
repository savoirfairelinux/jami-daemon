subdir('media')
subdir('ringdht')
subdir('sip')

libjami_libs = [libmedia, libringdht, libsip]

if depupnp.found()
    subdir('upnp')
    libjami_libs += [libupnp]
endif

libjami_includedirs = include_directories(
    'client',
    'config',
    'dring',
    'hooks',
    'im',
    'security'
)

libjami_sources = [
    'account.cpp',
    'account_factory.cpp',
    'archiver.cpp',
    'base64.cpp',
    'buildinfo.cpp',
    'call.cpp',
    'call_factory.cpp',
    'client/callmanager.cpp',
    'client/configurationmanager.cpp',
    'client/datatransfer.cpp',
    'client/presencemanager.cpp',
    'client/ring_signal.cpp',
    'client/videomanager.cpp',
    'conference.cpp',
    'config/yamlparser.cpp',
    'data_transfer.cpp',
    'fileutils.cpp',
    'ftp_server.cpp',
    'hooks/urlhook.cpp',
    'ice_transport.cpp',
    'im/instant_messaging.cpp',
    'im/message_engine.cpp',
    'ip_utils.cpp',
    'logger.cpp',
    'manager.cpp',
    'peer_connection.cpp',
    'preferences.cpp',
    'ring_api.cpp',
    'scheduled_executor.cpp',
    'security/certstore.cpp',
    'security/diffie-hellman.cpp',
    'security/memory.cpp',
    'security/tls_session.cpp',
    'security/tlsvalidator.cpp',
    'smartools.cpp',
    'string_utils.cpp',
    'threadloop.cpp',
    'thread_pool.cpp',
    'turn_transport.cpp',
    'utf8_utils.cpp'
]

if get_option('interfaces').contains('library')
    libjami = shared_library('ring',
        sources: libjami_sources,
        include_directories: libjami_includedirs,
        dependencies: [
            depgnutls,
            depyaml-cpp,
            depjsoncpp,
            deplibsecp256k1,
            depzlib
        ],
        link_with: libjami_libs,
        install: true
    )

    if host_machine.system() == 'linux'
        pkg = import('pkgconfig')
        pkg.generate(
            name: 'jami',
            description: 'jami library',
            libraries: libjami
        )
    endif
else
    libjami = static_library('ring',
        sources: libjami_sources,
        include_directories: libjami_includedirs,
        dependencies: [
            depgnutls,
            depyaml-cpp,
            depjsoncpp,
            deplibsecp256k1,
            depzlib
        ],
        link_with: libjami_libs
    )
endif
