# MSGPACK
MSGPACK_VERSION := cpp-1.2.0
MSGPACK_URL := https://github.com/msgpack/msgpack-c/archive/$(MSGPACK_VERSION).tar.gz

PKGS += msgpack
ifeq ($(call need_pkg,"msgpack >= 1.1"),)
PKGS_FOUND += msgpack
endif

MSGPACK_CMAKECONF := -DMSGPACK_CXX11=ON \
		-DCMAKE_INSTALL_LIBDIR=lib

$(TARBALLS)/msgpack-c-$(MSGPACK_VERSION).tar.gz:
	$(call download,$(MSGPACK_URL))

.sum-msgpack: msgpack-c-$(MSGPACK_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

msgpack: msgpack-c-$(MSGPACK_VERSION).tar.gz .sum-msgpack
	$(UNPACK)
	$(MOVE)

.msgpack: msgpack toolchain.cmake
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@
