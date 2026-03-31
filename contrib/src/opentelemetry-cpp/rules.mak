# OPENTELEMETRY-CPP
OPENTELEMETRY_CPP_VERSION := 1.19.0
OPENTELEMETRY_CPP_URL := https://github.com/open-telemetry/opentelemetry-cpp/archive/v$(OPENTELEMETRY_CPP_VERSION).tar.gz

PKGS += opentelemetry-cpp
ifeq ($(call need_pkg,'opentelemetry-cpp >= 1.19.0'),)
PKGS_FOUND += opentelemetry-cpp
endif

OPENTELEMETRY_CPP_CONF = -DCMAKE_CXX_STANDARD=20 \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_TESTING=OFF \
	-DWITH_OTLP_GRPC=OFF \
	-DWITH_PROMETHEUS=OFF \
	-DWITH_EXAMPLES=OFF \
	-DWITH_FUNC_TESTS=OFF \
	-DOPENTELEMETRY_BUILD=ON

# Enable the OTLP HTTP exporter only when the daemon's otel export feature is
# requested (mirrors the JAMI_ENABLE_OTEL_EXPORT CMake option / meson
# otel_export option).  When ON, protobuf is required to compile the OTLP
# proto sources that opentelemetry-cpp ships pre-generated.
ifdef JAMI_ENABLE_OTEL_EXPORT
OPENTELEMETRY_CPP_CONF += -DWITH_OTLP_HTTP=ON
# When cross-compiling, opentelemetry-cpp's proto generation needs the HOST
# protoc binary.  Since the contrib protobuf is built without protoc binaries
# (cross-target only), we must supply Protobuf_PROTOC_EXECUTABLE explicitly so
# that cmake/opentelemetry-proto.cmake's cross-compile branch picks it up.
OPENTELEMETRY_CPP_CONF += -DProtobuf_PROTOC_EXECUTABLE=$(shell which protoc)
DEPS_opentelemetry-cpp = protobuf curl nlohmann_json
else
OPENTELEMETRY_CPP_CONF += -DWITH_OTLP_HTTP=OFF
endif

$(TARBALLS)/opentelemetry-cpp-$(OPENTELEMETRY_CPP_VERSION).tar.gz:
	$(call download,$(OPENTELEMETRY_CPP_URL))

.sum-opentelemetry-cpp: opentelemetry-cpp-$(OPENTELEMETRY_CPP_VERSION).tar.gz

opentelemetry-cpp: opentelemetry-cpp-$(OPENTELEMETRY_CPP_VERSION).tar.gz .sum-opentelemetry-cpp
	$(UNPACK)
	$(MOVE)

CMAKE_PKGS += opentelemetry-cpp
