# OpenTelemetry — self-contained build fragment.
# All dependency lookups, source files, and feature flags for telemetry live
# here.  To add or remove an otel component, only this file needs changing
# (plus meson_options.txt for the option declarations).
#
# Exposes to the parent scope via ordinary variables (Meson subdir convention):
#   telemetry_sources  — source files to add to libjami
#   telemetry_deps     — dependency objects to add to libjami
#   telemetry_incdirs  — include_directories to add to libjami

telemetry_sources = []
telemetry_deps    = []
telemetry_incdirs = []

if get_option('otel')
    telemetry_sources = files(
        'telemetry.cpp',
        'telemetry_runtime.cpp',
        'trace_types.cpp',
        'trace_store.cpp',
        'trace_export.cpp',
        'ring_buffer_span_exporter.cpp',
        'otel_recordable_bridge.cpp',
        'otlp_uploader.cpp',
    )
    telemetry_incdirs = [include_directories('.')]

    _depotel_api       = dependency('opentelemetry_api')
    _depotel_trace     = dependency('opentelemetry_trace')
    _depotel_logs      = dependency('opentelemetry_logs')
    _depotel_metrics   = dependency('opentelemetry_metrics')
    _depotel_resources = dependency('opentelemetry_resources')
    _depotel_common    = dependency('opentelemetry_common')

    telemetry_deps = [
        _depotel_api,
        _depotel_trace,
        _depotel_logs,
        _depotel_metrics,
        _depotel_resources,
        _depotel_common,
    ]

    if get_option('otel_export')
        # Meson doesn't understand $<LINK_ONLY:...> generator expressions, so
        # transitive deps expressed that way in the opentelemetry-cpp cmake config
        # are silently dropped.  The affected chain is:
        #   otlp_http_exporter → otlp_http_client
        #     → $<LINK_ONLY:http_client_curl>
        #       → $<LINK_ONLY:CURL::libcurl_static>
        # Request them explicitly.
        _depotel_otlp_http = dependency('opentelemetry-cpp', method: 'cmake',
            modules: [
                'opentelemetry-cpp::otlp_http_exporter',
                'opentelemetry-cpp::http_client_curl',
            ])
        _depcurl = dependency('libcurl')
        telemetry_deps += [_depotel_otlp_http, _depcurl]
        # Use a compiler flag rather than conf.set() so this file stays
        # independent of the configure_file() call in the top-level meson.build.
        add_project_arguments('-DJAMI_OTEL_EXPORT_ENABLED', language: 'cpp')
    endif
endif
