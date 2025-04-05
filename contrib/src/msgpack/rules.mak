# MSGPACK
MSGPACK_VERSION_NUMBER := 7.0.0
MSGPACK_VERSION := cpp-$(MSGPACK_VERSION_NUMBER)
PKG_CPE += cpe:2.3:a:*:msgpack:$(MSGPACK_VERSION_NUMBER):*:*:*:*:*:*:*
MSGPACK_URL := https://github.com/msgpack/msgpack-c/archive/$(MSGPACK_VERSION).tar.gz

PKGS += msgpack
ifeq ($(call need_pkg,"msgpack >= 5.0.0"),)
PKGS_FOUND += msgpack
endif

MSGPACK_CMAKECONF := -DMSGPACK_CXX17=ON \
		-DMSGPACK_CXX_ONLY=ON \
		-DMSGPACK_USE_BOOST=Off \
		-DMSGPACK_BUILD_EXAMPLES=OFF \
		-DMSGPACK_ENABLE_SHARED=OFF \
		-DCMAKE_INSTALL_LIBDIR=lib

$(TARBALLS)/msgpack-c-$(MSGPACK_VERSION).tar.gz:
	$(call download,$(MSGPACK_URL))

.sum-msgpack: msgpack-c-$(MSGPACK_VERSION).tar.gz

msgpack: msgpack-c-$(MSGPACK_VERSION).tar.gz .sum-msgpack
	$(UNPACK)
	$(MOVE)

.msgpack: msgpack toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) . $(MSGPACK_CMAKECONF)
	cd $< && $(MAKE) install
	touch $@
