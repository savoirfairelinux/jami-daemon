################################################################################
# Source group — telemetry
################################################################################

list(APPEND Source_Files__telemetry
    "${CMAKE_CURRENT_SOURCE_DIR}/telemetry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/telemetry.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/telemetry_runtime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/telemetry_runtime.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/trace_types.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trace_types.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/trace_store.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trace_store.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/trace_export.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trace_export.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ring_buffer_span_exporter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ring_buffer_span_exporter.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/otel_recordable_bridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/otel_recordable_bridge.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/otlp_uploader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/otlp_uploader.h"
)

set(Source_Files__telemetry ${Source_Files__telemetry} PARENT_SCOPE)

################################################################################
# OpenTelemetry dependency bundle
#
# jami-telemetry-deps is an INTERFACE library that carries all OpenTelemetry
# link targets (and, when JAMI_ENABLE_OTEL_EXPORT is ON, the OTLP HTTP
# exporter + the matching compile definition).  Consumers only need to link
# against this one target; they don't need to know which otel components exist.
################################################################################

option(JAMI_ENABLE_OTEL_EXPORT "Enable OTLP HTTP export for OpenTelemetry" OFF)

if(NOT MSVC)
    if(JAMI_ENABLE_OTEL_EXPORT)
        find_package(nlohmann_json REQUIRED)
        find_package(Protobuf MODULE REQUIRED)
        find_package(ZLIB REQUIRED)
        find_package(CURL CONFIG REQUIRED)
    endif()
    find_package(opentelemetry-cpp CONFIG REQUIRED)
endif()

add_library(jami-telemetry-deps INTERFACE)

if(NOT MSVC)
    target_link_libraries(jami-telemetry-deps INTERFACE
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::logs
        opentelemetry-cpp::metrics
        opentelemetry-cpp::trace
    )
    if(JAMI_ENABLE_OTEL_EXPORT)
        target_link_libraries(jami-telemetry-deps INTERFACE
            opentelemetry-cpp::otlp_http_exporter
        )
        target_compile_definitions(jami-telemetry-deps INTERFACE
            JAMI_OTEL_EXPORT_ENABLED
        )
    endif()
endif()
