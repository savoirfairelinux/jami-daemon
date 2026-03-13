# OpenTelemetry C++ — CMake Integration Guide

| Field        | Value       |
|--------------|-------------|
| Status       | draft       |
| Last Updated | 2026-03-13  |

---

## 1. Minimum CMake Version

OpenTelemetry C++ SDK requires **CMake ≥ 3.25** (as of v1.25.0).

```cmake
cmake_minimum_required(VERSION 3.25)
```

---

## 2. FetchContent Approach (Preferred for Self-Contained Builds)

This approach fetches and builds the SDK as part of your project build. Recommended for jami-daemon to avoid system-level installation requirements.

```cmake
include(FetchContent)

FetchContent_Declare(
    opentelemetry-cpp
    GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-cpp.git
    GIT_TAG        v1.25.0          # pin to exact stable tag
    GIT_SHALLOW    TRUE
)

# Configure OTel build options BEFORE FetchContent_MakeAvailable
set(BUILD_TESTING             OFF CACHE BOOL "" FORCE)
set(OPENTELEMETRY_INSTALL     OFF CACHE BOOL "" FORCE)  # don't install into CMAKE_INSTALL_PREFIX
set(WITH_OTLP_GRPC            ON  CACHE BOOL "" FORCE)  # enable gRPC exporter
set(WITH_OTLP_HTTP            ON  CACHE BOOL "" FORCE)  # enable HTTP exporter
set(WITH_PROMETHEUS           OFF CACHE BOOL "" FORCE)  # optional
set(WITH_ZIPKIN               OFF CACHE BOOL "" FORCE)
set(WITH_JAEGER               OFF CACHE BOOL "" FORCE)
set(WITH_ABSEIL               OFF CACHE BOOL "" FORCE)  # use ON if absl is already a dep
set(WITH_STL                  ON  CACHE BOOL "" FORCE)  # prefer std:: over nostd::

FetchContent_MakeAvailable(opentelemetry-cpp)
```

> **Performance note**: FetchContent on a clean build will clone and compile the full SDK (~2–5 min). Use `FETCHCONTENT_UPDATES_DISCONNECTED=ON` in CI to cache.

---

## 3. find_package Approach (System-Installed OTel)

For distro packages or pre-installed SDKs (e.g., via vcpkg or Conan):

```cmake
find_package(opentelemetry-cpp CONFIG REQUIRED
    COMPONENTS
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::otlp_grpc_exporter
        opentelemetry-cpp::otlp_http_exporter
        opentelemetry-cpp::ostream_span_exporter
)

if (NOT opentelemetry-cpp_FOUND)
    message(FATAL_ERROR "OpenTelemetry C++ not found. Install libopentelemetry-cpp-dev or build from source.")
endif()
```

The `find_package` config file is installed at `<prefix>/lib/cmake/opentelemetry-cpp/opentelemetry-cppConfig.cmake`.

---

## 4. Required CMake Targets

| Target | Description | Required for |
|--------|-------------|-------------|
| `opentelemetry-cpp::api` | Header-only API; zero-dependency | All instrumented code |
| `opentelemetry-cpp::sdk` | Core SDK (processors, samplers) | Application main binary |
| `opentelemetry-cpp::resources` | Resource attribute helpers | All providers |
| `opentelemetry-cpp::trace` | Trace SDK | TracerProvider init |
| `opentelemetry-cpp::metrics` | Metrics SDK | MeterProvider init |
| `opentelemetry-cpp::logs` | Logs SDK | LoggerProvider init |
| `opentelemetry-cpp::ostream_span_exporter` | stdout trace exporter | Debug/dev only |
| `opentelemetry-cpp::ostream_metrics_exporter` | stdout metrics exporter | Debug/dev only |
| `opentelemetry-cpp::ostream_log_record_exporter` | stdout log exporter | Debug/dev only |
| `opentelemetry-cpp::otlp_grpc_exporter` | OTLP/gRPC trace exporter | Production |
| `opentelemetry-cpp::otlp_grpc_metrics_exporter` | OTLP/gRPC metrics exporter | Production |
| `opentelemetry-cpp::otlp_grpc_log_record_exporter` | OTLP/gRPC log exporter | Production |
| `opentelemetry-cpp::otlp_http_exporter` | OTLP/HTTP trace exporter | Alternative production |
| `opentelemetry-cpp::otlp_http_metric_exporter` | OTLP/HTTP metrics exporter | Alternative production |
| `opentelemetry-cpp::otlp_http_log_record_exporter` | OTLP/HTTP log exporter | Alternative production |
| `opentelemetry-cpp::prometheus_exporter` | Prometheus metrics scrape endpoint | Optional |

---

## 5. Key CMake Build Options

| Option | Default | Recommended | Notes |
|--------|---------|-------------|-------|
| `BUILD_TESTING` | `ON` | **OFF** | Disables test targets; speeds up build significantly |
| `WITH_OTLP_GRPC` | `OFF` | **ON** | Enables OTLP/gRPC exporter; requires gRPC + protobuf |
| `WITH_OTLP_HTTP` | `OFF` | `ON` or `OFF` | Enables OTLP/HTTP exporter; requires libcurl or built-in http client |
| `WITH_PROMETHEUS` | `OFF` | Optional | Enables Prometheus pull exporter |
| `WITH_ABSEIL` | `OFF` | Match project | Set `ON` if the project already uses Abseil; avoids duplicate |
| `WITH_STL` | `OFF` | **ON** | Use `std::` containers instead of `nostd::` polyfills; requires C++17 |
| `OPENTELEMETRY_INSTALL` | `ON` | `OFF` (FetchContent) | Skip installing OTel headers into system dirs when using FetchContent |
| `WITH_ZIPKIN` | `OFF` | `OFF` | Not needed for jami |
| `WITH_JAEGER` | `OFF` | `OFF` | Jaeger now uses OTLP natively |
| `BUILD_W3CTRACECONTEXT_TEST` | `OFF` | `OFF` | W3C compliance tests |

---

## 6. OTLP Exporter Dependencies (gRPC / Protobuf)

The OTLP gRPC exporter requires:
- **gRPC** ≥ 1.43 (with C++ plugin)
- **protobuf** ≥ 3.21 (Protocol Buffers)
- **opentelemetry-proto** (auto-fetched by the SDK's CMake)

### Finding or building gRPC

```cmake
# Option A: system gRPC
find_package(gRPC CONFIG REQUIRED)

# Option B: FetchContent gRPC (large; not recommended unless you need a specific version)
# See contrib/grpc in jami-daemon for existing approach
```

For jami-daemon, gRPC is likely already available via `contrib/`. Check `contrib/src/` for an existing gRPC recipe.

### Linking the exporter

```cmake
target_link_libraries(jami-daemon-otel PRIVATE
    opentelemetry-cpp::api
    opentelemetry-cpp::sdk
    opentelemetry-cpp::resources
    opentelemetry-cpp::trace
    opentelemetry-cpp::metrics
    opentelemetry-cpp::logs
    opentelemetry-cpp::otlp_grpc_exporter
    opentelemetry-cpp::otlp_grpc_metrics_exporter
    opentelemetry-cpp::otlp_grpc_log_record_exporter
)
```

---

## 7. Making OTel Optional — Compile-Time Feature Flag

### CMakeLists.txt (top-level or feature module)

```cmake
# Top-level option
option(WITH_OPENTELEMETRY "Enable OpenTelemetry C++ instrumentation" OFF)

if(WITH_OPENTELEMETRY)
    include(FetchContent)
    FetchContent_Declare(
        opentelemetry-cpp
        GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-cpp.git
        GIT_TAG        v1.25.0
        GIT_SHALLOW    TRUE
    )
    set(BUILD_TESTING         OFF CACHE BOOL "" FORCE)
    set(WITH_OTLP_GRPC        ON  CACHE BOOL "" FORCE)
    set(OPENTELEMETRY_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(opentelemetry-cpp)

    # Generate a config header visible to C++ code
    target_compile_definitions(jami-daemon PRIVATE JAMI_WITH_OPENTELEMETRY=1)

    target_link_libraries(jami-daemon PRIVATE
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::otlp_grpc_exporter
    )
endif()
```

### config.h / feature header guard

```cpp
// In C++ source files:
#ifdef JAMI_WITH_OPENTELEMETRY
#  include "otel/otel_init.h"
#endif

// Usage:
#ifdef JAMI_WITH_OPENTELEMETRY
    jami::otel::Init(endpoint);
#endif
```

### No-op shim (API-only build)

When `WITH_OPENTELEMETRY=OFF`, you can still include `opentelemetry/trace/provider.h` from the API-only package (header-only). The global provider returns a no-op tracer, so instrumentation code compiles and runs without any overhead.

For zero-dependency builds (no OTel at all), wrap all OTel calls in `#ifdef JAMI_WITH_OPENTELEMETRY`.

---

## 8. Meson Integration (if needed)

jami-daemon also has a `meson.build`. If adding OTel support to the Meson build:

```meson
# meson.build
otel_dep = dependency('opentelemetry-cpp',
    required: get_option('with_opentelemetry'),
    fallback: ['opentelemetry-cpp', 'opentelemetry_cpp_dep'])
```

> **Note**: The OTel C++ SDK's Meson support is community-maintained and less polished than CMake. Prefer the CMake build for OTel integration even if the daemon uses Meson for other components — use a CMake ExternalProject or a pre-built package.

---

## Source References

- [opentelemetry-cpp INSTALL.md](https://github.com/open-telemetry/opentelemetry-cpp/blob/main/INSTALL.md)
- [opentelemetry-cpp CMakeLists.txt](https://github.com/open-telemetry/opentelemetry-cpp/blob/main/CMakeLists.txt)
- [OTel C++ Getting Started — CMake setup](https://opentelemetry.io/docs/languages/cpp/getting-started/)
- [examples/simple/CMakeLists.txt](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/examples/simple)
- [examples/otlp/CMakeLists.txt](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/examples/otlp)

---

## Open Questions

1. **gRPC availability in contrib/**: Does jami-daemon's existing `contrib/` system already build gRPC? If so, which version? `WITH_OTLP_GRPC=ON` must use a compatible version.
2. **Build time**: Does adding the OTel SDK to FetchContent significantly impact the CI build time? Consider caching the FetchContent population directory.
3. **WITH_STL vs nostd**: With `WITH_STL=ON` (C++17), the OTel API uses `std::string_view`, `std::shared_ptr`, etc. instead of `nostd::` polyfills. Does jami-daemon already require C++17?
4. **Meson primary build**: If the daemon's primary build is Meson (given `meson.build` at root), should OTel be a CMake sub-project or an external pre-built dependency (e.g., from a distro package or `contrib/`)?
5. **Static vs dynamic linking**: Should OTel be linked statically (simpler deployment) or dynamically (allows upgrading OTel without recompilation)? For an embedded daemon, static is typically preferred.
