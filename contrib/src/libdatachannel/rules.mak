# LIBDATACHANNEL
LIBDATACHANNEL_VERSION := 0.18.2
LIBDATACHANNEL_URL := https://github.com/paullouisageneau/libdatachannel/archive/refs/tags/v$(LIBDATACHANNEL_VERSION).tar.gz

PKGS += libdatachannel

ifeq ($(call need_pkg,"libdatachannel >= 0.18.2"),)
PKGS_FOUND += libdatachannel
endif

DEPS_libdatachannel += gnutls

LIBDATACHANNEL_CMAKECONF := \
		-DUSE_GNUTLS=0 \
		-DNO_WEBSOCKET=1 \
		-DCMAKE_BUILD_TYPE=Release \

$(TARBALLS)/libdatachannel-$(LIBDATACHANNEL_VERSION).tar.gz:
	$(call download,$(LIBDATACHANNEL_URL))

.sum-libdatachannel: libdatachannel-$(LIBDATACHANNEL_VERSION).tar.gz

libdatachannel: libdatachannel-$(LIBDATACHANNEL_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.libdatachannel: libdatachannel toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) . $(LIBDATACHANNEL_CMAKECONF)
	cd $< && $(MAKE) install
	touch $@
