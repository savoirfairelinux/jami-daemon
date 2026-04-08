################################################################################
# Source group — telemetry
################################################################################

list(APPEND Source_Files__telemetry
    "${CMAKE_CURRENT_SOURCE_DIR}/telemetry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/remote_tracer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/retry_span_exporter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/telemetry.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/retry_span_exporter.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/calls_trace.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/push_trace.h"
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
        find_package(Protobuf CONFIG REQUIRED)
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
        opentelemetry-cpp::in_memory_span_exporter
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
