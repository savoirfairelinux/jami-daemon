# PROTOBUF
# Version 3.21.12 is the last release that does not require abseil-cpp.
# The host protoc in the nix Android shells is pinned to the matching 3.21.x
# series (pkgs.protobuf_21) so generated .pb.cc files are ABI-compatible with
# this library.
PROTOBUF_VERSION := 3.21.12
PROTOBUF_URL := https://github.com/protocolbuffers/protobuf/archive/v$(PROTOBUF_VERSION).tar.gz

PROTOBUF_CONF = -DBUILD_SHARED_LIBS=OFF \
	-Dprotobuf_BUILD_TESTS=OFF \
	-Dprotobuf_BUILD_EXAMPLES=OFF \
	-Dprotobuf_WITH_ZLIB=OFF \
	-DCMAKE_BUILD_CMAKEDIR=lib/cmake/protobuf

# When cross-compiling, only build the runtime library for the target.
# The host protoc binary comes from the nix development shell.
ifdef HAVE_CROSS_COMPILE
PROTOBUF_CONF += -Dprotobuf_BUILD_PROTOC_BINARIES=OFF \
	-Dprotobuf_BUILD_LIBPROTOC=OFF
endif

$(TARBALLS)/protobuf-$(PROTOBUF_VERSION).tar.gz:
	$(call download,$(PROTOBUF_URL))

.sum-protobuf: protobuf-$(PROTOBUF_VERSION).tar.gz

protobuf: protobuf-$(PROTOBUF_VERSION).tar.gz .sum-protobuf
	$(UNPACK)
	$(MOVE)

CMAKE_PKGS += protobuf
