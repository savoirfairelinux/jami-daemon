################################################################################
# OpenTelemetry bootstrap integration smoke-test
#
# Build standalone (no jami-daemon library required):
#
#   cmake -S . -B _build \
#         -DENABLE_OTEL=ON \
#         -DOPENTELEMETRY_CPP_ROOT=/path/to/otel/install \
#         -DCMAKE_MODULE_PATH=/path/to/jami-daemon/CMake
#   cmake --build _build
#   ctest --test-dir _build --output-on-failure
#
# Or include this directory from the top-level jami-daemon build when the
# ENABLE_OTEL and BUILD_TESTING options are both ON.
################################################################################

cmake_minimum_required(VERSION 3.16)

# Allow this file to be used both as a standalone project and as a subdirectory
# of the top-level jami-daemon CMake build.
if (NOT PROJECT_NAME)
    project(jami-otel-test
        LANGUAGES CXX
        VERSION 0.0.1)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # Register the jami-daemon CMake/ directory so Findopentelemetry-cpp.cmake
    # is visible when this is built standalone.
    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake")
endif()

################################################################################
# OTel dependency
################################################################################
option(ENABLE_OTEL "Enable OpenTelemetry instrumentation" OFF)

if (ENABLE_OTEL)
    find_package(opentelemetry-cpp REQUIRED)

    if (NOT TARGET opentelemetry::api)
        message(FATAL_ERROR
            "opentelemetry::api target not defined after find_package. "
            "Set OPENTELEMETRY_CPP_ROOT or CMAKE_PREFIX_PATH.")
    endif()
endif()

################################################################################
# Helper macro — create one OTel smoke-test target
################################################################################
macro(add_otel_smoke_test TARGET_NAME SOURCES)
    add_executable(${TARGET_NAME} ${SOURCES} ../../src/otel/otel_init.cpp)

    target_include_directories(${TARGET_NAME}
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    if (ENABLE_OTEL)
        target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_OTEL)

        target_link_libraries(${TARGET_NAME}
            PRIVATE
                opentelemetry::api
                opentelemetry::sdk
                opentelemetry::ostream_span_exporter
                opentelemetry::ostream_metrics_exporter
                opentelemetry::ostream_log_record_exporter
        )

        # Conditionally link OTLP exporters if available.
        foreach(_exp
                otlp_grpc_exporter otlp_grpc_metrics_exporter otlp_grpc_log_record_exporter
                otlp_http_exporter otlp_http_metric_exporter otlp_http_log_record_exporter)
            if (TARGET opentelemetry::${_exp})
                target_link_libraries(${TARGET_NAME} PRIVATE opentelemetry::${_exp})
                if(_exp MATCHES "grpc")
                    target_compile_definitions(${TARGET_NAME} PRIVATE WITH_OTLP_GRPC)
                elseif(_exp MATCHES "http")
                    target_compile_definitions(${TARGET_NAME} PRIVATE WITH_OTLP_HTTP)
                endif()
            endif()
        endforeach()
    endif()
endmacro()

################################################################################
# Test executables
################################################################################

# ── otel_basic_test ──────────────────────────────────────────────────────────
# Basic OTel bootstrap smoke-test (spans, meter, logger).
add_otel_smoke_test(otel_basic_test otel_basic_test.cpp)

# ── call_metrics_test ────────────────────────────────────────────────────────
# Exercises the CallMetrics singleton instruments (counters, histograms).
# call_metrics.cpp is embedded directly into the test TU via #include
# (see call_metrics_test.cpp) to avoid building the full jami-daemon library.
add_otel_smoke_test(call_metrics_test call_metrics_test.cpp)

# ── jaeger_e2e_test ──────────────────────────────────────────────────────────
# End-to-end test: send spans to Jaeger via OTLP HTTP.
add_otel_smoke_test(jaeger_e2e_test jaeger_e2e_test.cpp)

################################################################################
# CTest integration
################################################################################
include(CTest)
enable_testing()

add_test(
    NAME    otel_basic_test
    COMMAND otel_basic_test
)
set_tests_properties(otel_basic_test PROPERTIES
    PASS_REGULAR_EXPRESSION "OTEL_TEST_PASS"
    TIMEOUT 30
)

add_test(
    NAME    call_metrics_test
    COMMAND call_metrics_test
)
set_tests_properties(call_metrics_test PROPERTIES
    PASS_REGULAR_EXPRESSION "CALL_METRICS_TEST_PASS"
    TIMEOUT 30
)

################################################################################
# log_bridge_test — OTel logging bridge smoke-test
#
# Unlike the other smoke-tests this target also compiles logger.cpp (the full
# jami log dispatch infrastructure) and otel_log_bridge.cpp.
# logger.cpp transitively includes client/jami_signal.h which needs all the
# jami public interface headers under src/jami/.  Both include roots are added
# below.  When building inside the top-level jami-daemon CMake tree all
# necessary libraries (fmt, opendht) are already present; for a standalone
# build they must be located via find_package / pkg-config.
################################################################################

# Locate fmt (required by logger.cpp for fmt::format)
if (NOT TARGET fmt::fmt)
    find_package(fmt QUIET)
endif()

# Locate OpenDHT (required by logger.h: #include <opendht/logger.h>)
if (NOT TARGET opendht)
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(opendht IMPORTED_TARGET opendht)
    endif()
endif()

add_executable(log_bridge_test
    log_bridge_test.cpp
    ../../src/otel/otel_init.cpp
    ../../src/otel/otel_log_bridge.cpp
    ../../src/logger.cpp
)

target_include_directories(log_bridge_test
    PRIVATE
        # Primary include root: resolves "logger.h", "otel/otel_init.h", etc.
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
        # Public jami API headers needed by client/jami_signal.h
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/jami"
)

target_compile_features(log_bridge_test PRIVATE cxx_std_20)

if (ENABLE_OTEL)
    target_compile_definitions(log_bridge_test PRIVATE ENABLE_OTEL)

    target_link_libraries(log_bridge_test
        PRIVATE
            opentelemetry::api
            opentelemetry::sdk
            opentelemetry::ostream_span_exporter
            opentelemetry::ostream_metrics_exporter
            opentelemetry::ostream_log_record_exporter
    )

    foreach(_exp otlp_grpc_exporter otlp_http_exporter)
        if (TARGET opentelemetry::${_exp})
            target_link_libraries(log_bridge_test PRIVATE opentelemetry::${_exp})
        endif()
    endforeach()
endif()

# fmt — required for the jami logger.
if (TARGET fmt::fmt)
    target_link_libraries(log_bridge_test PRIVATE fmt::fmt)
elseif (TARGET fmt)
    target_link_libraries(log_bridge_test PRIVATE fmt)
endif()

# OpenDHT — needed at link time for logger.h's dhtLogger() helper.
if (TARGET opendht)
    target_link_libraries(log_bridge_test PRIVATE opendht)
elseif (TARGET PkgConfig::opendht)
    target_link_libraries(log_bridge_test PRIVATE PkgConfig::opendht)
endif()

add_test(
    NAME    log_bridge_test
    COMMAND log_bridge_test
)
set_tests_properties(log_bridge_test PROPERTIES
    PASS_REGULAR_EXPRESSION "LOG_BRIDGE_TEST_PASS"
    TIMEOUT 30
)
