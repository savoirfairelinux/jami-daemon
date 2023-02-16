# LIBDATACHANNEL
LIBDATACHANNEL_VERSION := v0.18.5
LIBDATACHANNEL_URL := https://github.com/paullouisageneau/libdatachannel.git

PKGS += libdatachannel

ifeq ($(call need_pkg,"libdatachannel >= 0.18.2"),)
PKGS_FOUND += libdatachannel
endif

$(TARBALLS)/libdatachannel-$(LIBDATACHANNEL_VERSION).tar.xz:
	$(call download_git,$(LIBDATACHANNEL_URL),$(LIBDATACHANNEL_VERSION),$(LIBDATACHANNEL_VERSION),,--recurse-submodules)

.sum-libdatachannel: libdatachannel-$(LIBDATACHANNEL_VERSION).tar.xz
	$(warning $@ not implemented)
	touch $@

.sum-libdatachannel: libdatachannel-$(LIBDATACHANNEL_VERSION).tar.xz

libdatachannel: libdatachannel-$(LIBDATACHANNEL_VERSION).tar.xz .sum-libdatachannel
	$(UNPACK)
	$(MOVE)

.libdatachannel: libdatachannel toolchain.cmake
	# Force download and unpacking. Actual build is done with an other tool, such as cmake.
