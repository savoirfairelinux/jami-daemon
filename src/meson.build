subdir('media')
subdir('ringdht')
subdir('sip')

libring_libs = [libmedia, libringdht, libsip]

if depupnp.found()
    subdir('upnp')
    libring_libs += [libupnp]
endif

libring_includedirs = include_directories(
    'client',
    'config',
    'dring',
    'hooks',
    'im',
    'security'
)

libring_sources = [
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
    libring = shared_library('ring',
        sources: libring_sources,
        include_directories: libring_includedirs,
        link_with: libring_libs,
        install: true
    )
else
    libring = static_library('ring',
        sources: libring_sources,
        include_directories: libring_includedirs,
        link_with: libring_libs
    )
endif
