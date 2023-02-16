# LIBDATACHANNEL
LIBDATACHANNEL_VERSION := v0.18.2
LIBDATACHANNEL_URL := https://github.com/paullouisageneau/libdatachannel.git

PKGS += libdatachannel

ifeq ($(call need_pkg,"libdatachannel >= 0.18.2"),)
PKGS_FOUND += libdatachannel
endif

DEPS_libdatachannel += gnutls

LIBDATACHANNEL_CMAKECONF := \
		-DUSE_GNUTLS=0 \
		-DNO_WEBSOCKET=1 \
		-DCMAKE_BUILD_TYPE=Release \

$(TARBALLS)/libdatachannel-$(LIBDATACHANNEL_VERSION).tar.xz:
	$(call download_git,$(LIBDATACHANNEL_URL),$(LIBDATACHANNEL_VERSION),$(LIBDATACHANNEL_VERSION),,--recurse-submodules)

.sum-libdatachannel: libdatachannel-$(LIBDATACHANNEL_VERSION).tar.xz

libdatachannel: libdatachannel-$(LIBDATACHANNEL_VERSION).tar.xz
	$(UNPACK)
	$(MOVE)

.libdatachannel: libdatachannel toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) . $(LIBDATACHANNEL_CMAKECONF)
	cd $< && $(MAKE) install
	touch $@
