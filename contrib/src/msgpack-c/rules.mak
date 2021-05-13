# MSGPACK
MSGPACK_VERSION := cpp-3.2.0
MSGPACK_URL := https://github.com/msgpack/msgpack-c/archive/$(MSGPACK_VERSION).tar.gz

PKGS += msgpack
ifeq ($(call need_pkg,"msgpack >= 1.1"),)
PKGS_FOUND += msgpack
endif

MSGPACK_CMAKECONF := -DMSGPACK_CXX11=ON \
		-DMSGPACK_BUILD_EXAMPLES=OFF \
		-DMSGPACK_ENABLE_SHARED=OFF \
		-DCMAKE_INSTALL_LIBDIR=lib

$(TARBALLS)/msgpack-c-$(MSGPACK_VERSION).tar.gz:
	$(call download,$(MSGPACK_URL))

.sum-msgpack: msgpack-c-$(MSGPACK_VERSION).tar.gz

msgpack: msgpack-c-$(MSGPACK_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.msgpack: msgpack toolchain.cmake .sum-msgpack
	cd $< && $(HOSTVARS) $(CMAKE) . $(MSGPACK_CMAKECONF)
	cd $< && $(MAKE) install
	touch $@
