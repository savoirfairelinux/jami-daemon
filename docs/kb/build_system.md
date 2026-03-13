# Build System & Dependency Management

**Status:** draft  
**Last Updated:** 2026-03-13

---

## Overview

jami-daemon maintains **two parallel build systems**:

| Build System | Role | Primary? |
|---|---|---|
| **CMake** (≥ 3.16) | Cross-platform, used for Android/iOS/macOS/Windows builds; the only system with full `contrib/` vendoring integration | **Primary for mobile/desktop app builds** |
| **Meson** (≥ 0.56) | Linux/desktop-first, used by distro packagers and the Jami CI; cleaner optional dependency handling via `meson_options.txt` | **Primary for Linux distribution builds** |

Both build systems are actively maintained and are reasonably in sync. The CMake build is the authoritative source for platform-specific code paths (MSVC, Android NDK, macOS fat binaries); the Meson build is simpler but lacks the `contrib/` vendoring step.

---

## Build Systems — CMake

### Version & Project Identity

```cmake
cmake_minimum_required(VERSION 3.16)
project(jami-core VERSION 16.0.0 LANGUAGES C CXX)
set(PROJECT_LABEL "libjami")
```

The built library target is named **`jami-core`**; the installed library and pkg-config file are named **`libjami`** / **`jami`**.

### Key Options (CMake Cache Variables)

| Option | Default | Description |
|---|---|---|
| `JAMI_PLUGIN` | `ON` | Plugin support (requires libarchive or minizip) |
| `JAMI_VIDEO` | `ON` | Video codec/capture support |
| `JAMI_VIDEO_ACCEL` | `ON` | Hardware video acceleration |
| `JAMI_DBUS` | `OFF` | D-Bus binding (sdbus-c++) |
| `JAMI_JNI` | `OFF` | JNI/Android binding (SWIG) |
| `JAMI_NODEJS` | `OFF` | Node.js binding (SWIG + node-gyp) |
| `BUILD_CONTRIB` | `ON` | Trigger `contrib/` vendored build at configure time |
| `BUILD_SHARED_LIBS` | `OFF` | Build as shared library |
| `BUILD_EXTRA_TOOLS` | `OFF` | Build `extras/tools` (autotools helpers) |
| `ENABLE_ASAN` | `OFF` | AddressSanitizer flags |
| `ENABLE_COVERAGE` | `OFF` | gcov/lcov coverage flags (see `CMake/coverage.cmake`) |
| `IGNORE_SYSTEM_LIBS` | *(unset)* | Pass `--ignore-system-libs` to `contrib/bootstrap`, forcing all deps from vendored source |

### Structure

```
CMakeLists.txt              ← top-level; handles contrib and creates the jami-core target
src/CMakeLists.txt          ← collects Source_Files lists; delegates to subdirs
src/media/CMakeLists.txt    ← media (audio/video) sources
src/sip/CMakeLists.txt      ← SIP layer sources
src/jamidht/CMakeLists.txt  ← Jami DHT account + swarm/eth crypto sources
CMake/coverage.cmake        ← lcov/gcovr coverage helper functions
CMake/Default.cmake         ← MSVC property defaults
CMake/DefaultCXX.cmake      ← MSVC C++ property defaults
CMake/Utils.cmake           ← use_props() helper for MSVC
```

`src/CMakeLists.txt` only **appends to list variables** (e.g., `Source_Files`, `Source_Files__media`); the actual `add_library()` and all `target_*()` calls are in the top-level `CMakeLists.txt`.

### Dependency Resolution (non-MSVC)

1. `CONTRIB_PATH` is set to `contrib/<triple>/` (e.g., `contrib/x86_64-unknown-linux-gnu/`).
2. `CMAKE_FIND_ROOT_PATH` and `CMAKE_PREFIX_PATH` are prepended with `CONTRIB_PATH` so that `find_package` and `pkg_search_module` prefer vendored libs.
3. All deps use `pkg_search_module(...  IMPORTED_TARGET <name>)` → `PkgConfig::<name>` targets, except `yaml-cpp` which uses `find_package(yaml-cpp CONFIG REQUIRED)`.

### Dependency Resolution (MSVC)

All deps are linked by absolute `.lib` paths under `contrib/build/` and `contrib/msvc/lib/x64/`. No `find_package` is used.

---

## Build Systems — Meson

### Version & Project Identity

```meson
project('jami-daemon', ['c', 'cpp'],
    version: '16.0.0',
    license: 'GPL3+',
    default_options: ['cpp_std=gnu++20', 'buildtype=debugoptimized'],
    meson_version: '>= 0.56',
)
```

On Darwin, `objcpp` is added as an additional language for CoreAudio/AVFoundation glue code.

### Key Options (`meson_options.txt`)

| Option | Type | Default | Description |
|---|---|---|---|
| `interfaces` | `array` | `['library']` | Enabled interfaces: `library`, `dbus`, `nodejs` |
| `video` | `boolean` | `true` | Video support |
| `hw_acceleration` | `boolean` | `true` | HW video acceleration |
| `plugins` | `boolean` | `true` | Plugin support |
| `name_service` | `feature` | `auto` | Name lookup service |
| `aaudio` | `feature` | `auto` | AAudio (Android) |
| `alsa` | `feature` | `auto` | ALSA audio |
| `pulseaudio` | `feature` | `auto` | PulseAudio |
| `jack` | `feature` | `auto` | JACK audio |
| `portaudio` | `feature` | `auto` | PortAudio (Windows) |
| `upnp` | `feature` | `auto` | UPnP NAT traversal |
| `natpmp` | `feature` | `auto` | NAT-PMP |
| `webrtc_ap` | `feature` | `auto` | WebRTC audio processing |
| `speex_ap` | `feature` | `auto` | Speex DSP audio processing |
| `tests` | `boolean` | `false` | Build test suite (requires cppunit ≥ 1.12) |
| `tracepoints` | `boolean` | `false` | LTTng-UST tracepoints (requires lttng-ust ≥ 2.13) |

`feature`-typed options follow standard Meson semantics: `auto` probes the system; `enabled` fails if missing; `disabled` skips unconditionally.

### Global Compile Definitions (Meson)

```meson
add_project_arguments('-DASIO_STANDALONE', language: ['c', 'cpp'])
add_project_arguments('-DMSGPACK_NO_BOOST', language: ['c', 'cpp'])
add_project_arguments('-DHAVE_CONFIG_H', language: ['c', 'cpp'])
add_project_arguments('-DLIBJAMI_BUILD', language: ['c', 'cpp'])
```

A `config.h` is generated via Meson's `configuration_data()` and placed in the build directory. Both build systems use `HAVE_CONFIG_H` to pull in this file.

---

## Dependency Management Strategy

jami-daemon uses a **three-tier vendoring strategy**:

### Tier 1 — `contrib/` Vendored Build (authoritative)

The `contrib/` directory contains a GNU-Make–based meta-build system (bootstrapped by `contrib/bootstrap`):

- `contrib/src/<name>/` — one directory per dependency containing a `rules.mak` (and sometimes patches).
- `contrib/bootstrap` — shell script that generates a top-level `Makefile` in `contrib/build-<triple>/` targeting `contrib/<triple>/`.
- `make -C contrib/build-<triple>/` downloads source tarballs (or uses `contrib/tarballs/`), configures, patches, and installs static/shared libs under `contrib/<triple>/lib/`, `contrib/<triple>/include/`, etc.

CMake wraps this by doing `execute_process(COMMAND make ...)` inside `if(BUILD_CONTRIB)` at **configure time**, so a full contrib build happens before any source is compiled.

For macOS universal binaries, CMake builds each architecture's contrib separately then uses `lipo` to merge `.a` files into a fat contrib under `contrib/apple-darwin<version>/`.

For Windows/MSVC, a Python script (`extras/scripts/winmake.py`) replaces the `make` step.

### Tier 2 — System / pkg-config Fallback

If `BUILD_CONTRIB=OFF` (CMake) or on pure system builds (Meson), all deps are located via `pkg_search_module` / `dependency()` from the system's pkg-config database.

Meson never invokes the contrib build — it always expects headers and libraries to be pre-installed (either from the system or from a separately-run contrib build that exports pkg-config `.pc` files).

### Tier 3 — Header-only / Inline (in contrib/src)

Several lighter deps (asio, msgpack, restinio, llhttp, http_parser) are effectively header-only and live under `contrib/src/`. They are installed into `contrib/<triple>/include/` by the contrib Makefile.

### No CMake FetchContent

There is **no** use of CMake `FetchContent`, `ExternalProject_Add`, or git submodules. All source management is done by the `contrib/` Makefile system.

---

## Major Dependencies

| Dependency | Min Version | contrib/src dir | CMake Target | Meson dep var | Purpose |
|---|---|---|---|---|---|
| **opendht** | ≥ 2.1.0 | `opendht/` | `PkgConfig::opendht` | `depopendht` | DHT overlay network |
| **dhtnet** | any | `dhtnet/` | `PkgConfig::dhtnet` | `depdhtnet` | ICE/TURN transport layer for DHT |
| **pjproject** | any (fork) | `pjproject/` | `PkgConfig::pjproject` | `deplibpjproject` | SIP stack (custom Jami fork) |
| **gnutls** | ≥ 3.6.7 | `gnutls/` | `PkgConfig::gnutls` | `depgnutls` | TLS/DTLS |
| **nettle** | ≥ 3.0.0 | `nettle/` | `PkgConfig::nettle` | `depnettle` | Cryptographic primitives |
| **libgit2** | ≥ 1.1.0 | `libgit2/` | `PkgConfig::git2` | `deplibgit2` | Conversation store (git-backed) |
| **secp256k1** | ≥ 0.1 | `secp256k1/` | `PkgConfig::secp256k1` | `deplibsecp256k1` | Ethereum-compatible key crypto |
| **ffmpeg** (libav*) | avcodec ≥ 56.60.100 | `ffmpeg/` | `PkgConfig::avcodec` etc. | `deplibavcodec` etc. | Audio/video encode/decode |
| **libx264** | any | (in ffmpeg contrib) | via libavcodec | — | H.264 video codec |
| **libvpx** | any | (in ffmpeg contrib) | via libavcodec | — | VP8/VP9 video codec |
| **opus** | any | `opus/` | via libavcodec | — | Opus audio codec |
| **fmt** | ≥ 5.3 | `fmt/` | `PkgConfig::fmt` | `depfmt` | Structured formatting (used in logger) |
| **yaml-cpp** | ≥ 0.5.1 | `yaml-cpp/` | `yaml-cpp::yaml-cpp` | `depyamlcpp` | Config file parsing |
| **jsoncpp** | ≥ 1.6.5 | `jsoncpp/` | `PkgConfig::jsoncpp` | `depjsoncpp` | JSON parsing |
| **simdutf** | any | `simdutf/` | `PkgConfig::simdutf` | `depsimdutf` | Fast UTF-8/16/32 conversion |
| **zlib** | any | `zlib/` | *(system)* | `depzlib` | Compression |
| **asio** | any | `asio/` | *(header-only, `ASIO_STANDALONE`)* | — | Async I/O |
| **msgpack-c** | any | (in contrib) | *(header-only, `MSGPACK_NO_BOOST`)* | — | Serialisation |
| **restinio** | any | `restinio/` | *(header-only)* | — | HTTP/WebSocket in opendht proxy |
| **webrtc-audio-processing** | any | `webrtc-audio-processing/` | `PkgConfig::webrtcap` | `depwebrtcap` | Echo cancellation / NS (optional) |
| **speexdsp** | any | `speexdsp/` | `PkgConfig::speexdsp` | `depspeexdsp` | Alternative echo cancellation (optional) |
| **libarchive** | ≥ 3.4.0 | `libarchive/` | `PkgConfig::archive` | `deplibarchive` | Plugin zip loading (non-Apple) |
| **minizip** | ≥ 3.0.0 | `minizip/` | `PkgConfig::archive` | `depminizip` | Plugin zip loading (Apple) |
| **alsa** | ≥ 1.0 | `jack/` + system | `PkgConfig::alsa` | `depalsa` | Linux audio (optional) |
| **libpulse** | ≥ 0.9.15 | — | `PkgConfig::pulseaudio` | `deplibpulse` | Linux audio (optional) |
| **jack** | any | `jack/` | `PkgConfig::jack` | `depjack` | Linux pro-audio (optional) |
| **portaudio** | any | `portaudio/` | `PkgConfig::portaudio` | `depportaudio` | Windows audio |
| **openssl** | any | `openssl/` | *(via opendht/dhtnet)* | `depopenssl` | TLS (Windows/Android, behind gnutls on Linux) |
| **lttng-ust** | ≥ 2.13 | `lttng-ust/` | *(direct link)* | `deplttngust` | LTTng user-space tracepoints (optional) |
| **sdbus-c++** | ≥ 2.0.0 | `sdbus-cpp/` | `PkgConfig::DBusCpp` | `depsdbuscpp` | D-Bus interface binding (optional) |
| **argon2** | any | `argon2/` | *(static .lib, MSVC)* | — | Password hashing (account archives) |
| **onnx** | any | `onnx/` | — | — | ML inference (plugin / AI features) |
| **opencv** | any | `opencv/` | — | — | Computer vision (plugin / AI features) |
| **liburcu** | any | `liburcu/` | — | — | Lock-free data structures (optional) |
| **natpmp** | any | `natpmp/` | — | — | NAT-PMP traversal (optional) |
| **upnp** | any | `upnp/` | — | — | UPnP traversal (optional) |
| **gmp** | any | `gmp/` | — | — | Multi-precision arithmetic (for gnutls) |

---

## Platform Targets

| Platform | Build System | Notes |
|---|---|---|
| **Linux (x86-64, arm64)** | CMake + Meson | Full feature set; udev required for video (V4L2); PulseAudio or ALSA or JACK required for audio |
| **Android (arm64-v8a, armeabi-v7a, x86, x86_64)** | CMake only | Cross-compile via NDK toolchain file; AAudio for audio; ABI-to-triple mapping in `CMakeLists.txt`; JNI binding via SWIG |
| **macOS (x86-64 + arm64 universal)** | CMake + partial Meson | VideoToolbox/CoreVideo/CoreAudio frameworks; fat-binary contrib merge via `lipo`; ObjC++ files (`.mm`) for CoreAudio/AVFoundation |
| **iOS** | CMake only | CoreAudio iOS path (`corelayer.mm`); video capture via `iosvideo/` |
| **Windows (MSVC, x64)** | CMake only | WinSDK ≥ 10.0.26100.0; contrib via `extras/scripts/winmake.py`; PortAudio for audio; all deps linked via absolute `.lib` paths; UWP/WinVideo for camera |

No CI configuration files (`.gitlab-ci.yml`, `.github/workflows/`) were found in the workspace. CI is likely defined in a separate infrastructure repository or not committed to this tree.

---

## C++ Standard

Both build systems require **C++20**:

- CMake: `set(CMAKE_CXX_STANDARD 20)` + `set(CMAKE_CXX_STANDARD_REQUIRED ON)`
- Meson: `default_options: ['cpp_std=gnu++20', ...]`

Meson uses `gnu++20` (enabling GNU extensions), CMake uses plain `20`. In practice the code uses GNU extensions freely (e.g., `__attribute__((format(...)))`), so `gnu++20` is the effective minimum.

---

## Feature Flags

### CMake Pattern

Boolean `option()` variables control conditional compilation:

```cmake
option(JAMI_VIDEO "Build with video support" ON)
# → target_compile_definitions(jami-core PRIVATE ENABLE_VIDEO)

option(JAMI_PLUGIN "Build with plugin support" ON)
# → target_compile_definitions(jami-core PRIVATE ENABLE_PLUGIN)
```

Optional library presence is detected via `pkg_search_module` (no `REQUIRED` keyword), then:

```cmake
if(webrtcap_FOUND)
    target_sources(jami-core PRIVATE ${Source_Files__media__audio__webrtc})
    target_link_libraries(jami-core PRIVATE PkgConfig::webrtcap)
    target_compile_definitions(jami-core PRIVATE HAVE_WEBRTC_AP)
endif()
```

### Meson Pattern

`feature`-typed options are paired with `conf.set10()` into `config.h`:

```meson
depwebrtcap = dependency('webrtc-audio-processing', required: get_option('webrtc_ap'))
conf.set10('HAVE_WEBRTC_AP', depwebrtcap.found())
```

Boolean options use direct `conf.set()`:

```meson
if get_option('video')
    conf.set('ENABLE_VIDEO', true)
endif
```

All flags land in `config.h` (included project-wide via `-DHAVE_CONFIG_H`) and are tested with `#ifdef ENABLE_VIDEO` / `#if HAVE_WEBRTC_AP` in source.

---

## Existing Telemetry / Logging Infrastructure

### 1. Custom `jami::Logger` (`src/logger.h`)

The primary logging mechanism. Key characteristics:

- **Not spdlog, glog, or systemd journal** — it is a hand-rolled class.
- Backed by `{fmt}` for formatting (`#include <fmt/core.h>`, `fmt::format`).
- Dispatches to platform-appropriate sink:
  - **Android**: `android/log.h` (`ANDROID_LOG_*`)
  - **Windows**: `winsyslog.h` / Windows Event Log
  - **Linux/macOS**: POSIX `syslog.h` levels (`LOG_DEBUG`, `LOG_INFO`, `LOG_WARNING`, `LOG_ERR`)
- Output channels are runtime-configurable: `setConsoleLog()`, `setSysLog()`, `setMonitorLog()`, `setFileLog(path)`.
- Exposes both printf-style macros and `{fmt}`-style macros:

```cpp
// printf-style (legacy)
JAMI_DBG("%s connected", peer.c_str());

// fmt-style (preferred, compile-time format check)
JAMI_LOG("peer {} connected", peer);
JAMI_DEBUG("rtt = {} ms", rtt);
JAMI_WARNING("buffer overrun in {}", module);
JAMI_ERROR("fatal: {}", reason);

// No-newline variants
JAMI_XINFO("partial line {}", x);
```

- Thread-safe write through `Logger::write(level, file, line, linefeed, tag, message)`.
- Bridges into opendht's logger via `Logger::dhtLogger(tag)`.

### 2. LTTng-UST Tracepoints (`src/jami/tracepoint-def.h`, `src/jami/trace-tools.h`)

An optional, **Linux-only** kernel/user-space tracing facility:

- Enabled by `-Dtracepoints=true` (Meson) / no direct CMake option found — tied to the Meson build.
- Requires `lttng-ust ≥ 2.13`.
- Defines tracepoints in the `jami` LTTng provider (e.g., `jami_tracepoint(emit_signal_begin_callback, ...)`).
- Used in the signal-dispatch hot path (`src/client/jami_signal.h`).
- **Not** a metrics/telemetry system — it is a low-overhead kernel-tracing integration for performance debugging.

### 3. No Structured Metrics / Telemetry

There is **no** existing:
- OpenTelemetry integration
- Prometheus metrics endpoint
- StatsD/InfluxDB push
- Structured JSON log sink
- Sampling profiler hooks

All performance visibility today is through LTTng tracepoints (Linux dev builds) or logs.

---

## Recommended OpenTelemetry-cpp Integration Approach

Given the analysis above, the recommended approach is:

### Why NOT FetchContent

- The project has no `FetchContent` precedent; all deps go through `contrib/`.
- `FetchContent` at configure time would conflict with `BUILD_CONTRIB`'s `execute_process` approach and create ordering issues.
- OpenTelemetry-cpp has many sub-dependencies (protobuf, grpc, nlohmann-json, etc.) that would require recursive `FetchContent`, making the build fragile.

### Why NOT contrib/ vendoring (initially)

- Adding a new entry to `contrib/src/` requires writing a `rules.mak` and ensuring it works for all five platforms (Linux, Android, macOS, iOS, Windows).
- OpenTelemetry-cpp's gRPC/protobuf exporter stack is heavy; the simpler OTLP-HTTP or stdout exporter is enough for a first integration.
- Contrib vendoring is the right long-term answer for mobile/hermetic builds, but is overkill for an initial optional feature on Linux/desktop.

### Recommended: pkg-config / find_package, optional, contrib later

**Phase 1 (Linux/desktop):** Treat opentelemetry-cpp as an optional system dependency, following the exact pattern used for `webrtc-audio-processing`:

- Query via `pkg_search_module(opentelemetry-cpp IMPORTED_TARGET opentelemetry-cpp)` (CMake) or `dependency('opentelemetry-cpp', required: get_option('otel'))` (Meson).
- Add a `JAMI_OTEL` / `otel` feature flag.
- Compile-define `HAVE_OPENTELEMETRY` when found.
- Link `PkgConfig::opentelemetry-cpp` to `jami-core`.

**Phase 2 (all platforms):** Add `contrib/src/opentelemetry-cpp/rules.mak` to vendor it hermetically, the same way `dhtnet`, `opendht`, and `simdutf` are vendored today.

---

## Adding OTel as an Optional Dependency — Concrete Steps

### Step 1: Add Meson option

In [meson_options.txt](../../meson_options.txt), add:

```meson
option('otel', type: 'feature', value: 'disabled',
       description: 'Enable OpenTelemetry-cpp SDK integration')
```

### Step 2: Probe the dependency in Meson build

In [meson.build](../../meson.build), inside the `# Optional dependencies` block:

```meson
depotel = dependency('opentelemetry-cpp', required: get_option('otel'))
conf.set10('HAVE_OPENTELEMETRY', depotel.found())
```

### Step 3: Add CMake option

In [CMakeLists.txt](../../CMakeLists.txt), add with the other `option()` lines:

```cmake
option(JAMI_OTEL "Build with OpenTelemetry-cpp support" OFF)
```

### Step 4: Probe the dependency in CMake

In `CMakeLists.txt`, inside the `# Check dependencies` block (after the contrib find-path setup):

```cmake
if(JAMI_OTEL AND NOT MSVC)
    pkg_search_module(opentelemetry-cpp IMPORTED_TARGET opentelemetry-cpp)
    if(opentelemetry-cpp_FOUND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENTELEMETRY)
        target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::opentelemetry-cpp)
        message(STATUS "OpenTelemetry-cpp found: ${opentelemetry-cpp_VERSION}")
    else()
        message(WARNING "JAMI_OTEL=ON but opentelemetry-cpp not found via pkg-config; disabling.")
    endif()
endif()
```

### Step 5: Guard source files with `#ifdef HAVE_OPENTELEMETRY`

Create `src/telemetry.h` / `src/telemetry.cpp` gated on `#ifdef HAVE_OPENTELEMETRY`. This keeps the zero-overhead path clean for builds without OTel.

### Step 6: Install opentelemetry-cpp on the build host

```bash
# Ubuntu/Debian (if packaged)
apt install libopentelemetry-cpp-dev

# Or build from source
cmake -DWITH_OTLP_HTTP=ON -DWITH_OTLP_GRPC=OFF \
      -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -B build/ opentelemetry-cpp/
cmake --build build/ --target install
```

Then configure jami-daemon with:

```bash
# CMake
cmake -DJAMI_OTEL=ON ..

# Meson
meson setup build/ -Dotel=enabled
```

---

## Source References

All files read during this analysis:

- [CMakeLists.txt](../../CMakeLists.txt) — top-level CMake (1073 lines)
- [meson.build](../../meson.build) — top-level Meson (189 lines)
- [meson_options.txt](../../meson_options.txt)
- [src/CMakeLists.txt](../../src/CMakeLists.txt)
- [src/media/CMakeLists.txt](../../src/media/CMakeLists.txt)
- [src/sip/CMakeLists.txt](../../src/sip/CMakeLists.txt)
- [src/jamidht/CMakeLists.txt](../../src/jamidht/CMakeLists.txt)
- [src/logger.h](../../src/logger.h) — logging infrastructure
- [src/jami/tracepoint-def.h](../../src/jami/tracepoint-def.h) — LTTng tracepoint definitions
- [src/jami/trace-tools.h](../../src/jami/trace-tools.h) — tracepoint macros
- [src/client/jami_signal.h](../../src/client/jami_signal.h) — tracepoint usage
- [CMake/coverage.cmake](../../CMake/coverage.cmake) — coverage helpers
- [CMake/Default.cmake](../../CMake/Default.cmake) — MSVC defaults
- [CMake/DefaultCXX.cmake](../../CMake/DefaultCXX.cmake) — MSVC C++ defaults
- [CMake/Utils.cmake](../../CMake/Utils.cmake) — MSVC use_props helper
- `contrib/src/` directory listing (44 vendored packages enumerated)

---

## Open Questions

1. **CI system location** — no `.gitlab-ci.yml` or `.github/workflows/` was found in this tree. CI is defined elsewhere. Which pipelines exercise CMake vs. Meson? Are Android / iOS builds CI-gated?

2. **Meson `tracepoints` option in CMake** — the `tracepoints=true` option and `lttng-ust` dependency exist only in the Meson build. There is no CMake equivalent. Is this intentional? Should CMake gain an `ENABLE_TRACEPOINTS` option?

3. **opentelemetry-cpp package name** — the pkg-config name varies by distro and OTel version (`opentelemetry-cpp`, `opentelemetry_cpp`). A `FindOpenTelemetry.cmake` module may be needed for portability.

4. **OTel exporter selection** — which exporter should be enabled by default: OTLP/HTTP (lighter, no gRPC), stdout (dev/debug only), or Jaeger? OTLP/HTTP avoids the protobuf/gRPC dependency tree and is the suggested starting point.

5. **Contrib recipe for OTel** — when vendoring in `contrib/src/opentelemetry-cpp/rules.mak`, only the OTLP-HTTP exporter should be enabled to avoid pulling in gRPC and protobuf as additional contrib dependencies.

6. **Android / iOS OTel** — OTel on mobile requires CMake contrib vendoring (Phase 2). The current OTel SDK has experimental Android support; iOS support is unverified in the upstream SDK.
