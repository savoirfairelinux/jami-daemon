
libupnp_includedirs = include_directories(
    '..'
)

libupnp_sources = [
    'upnp/upnp_context.cpp',
    'upnp/upnp_control.cpp',
    'upnp/upnp_igd.cpp'
]

libupnp = static_library('upnp',
    sources: libupnp_sources,
    include_directories: libupnp_includedirs
)
