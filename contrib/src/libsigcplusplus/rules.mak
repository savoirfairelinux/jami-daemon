LIBSIGCPLUSPLUS_VERSION := 06fabac512bf81164d5b01f877927cb39e9db864
LIBSIGCPLUSPLUS_URL := https://github.com/libsigcplusplus/libsigcplusplus/archive/$(LIBSIGCPLUSPLUS_VERSION).tar.gz

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += libsigcplusplus
endif
endif

ifeq ($(call need_pkg,'sigc++-2.0 >= 2.10.2'),)
PKGS_FOUND += libsigcplusplus
endif

DEPS_libsigcplusplus = mm-common

$(TARBALLS)/libsigcplusplus-$(LIBSIGCPLUSPLUS_VERSION).tar.gz:
	$(call download,$(LIBSIGCPLUSPLUS_URL))

.sum-libsigcplusplus: libsigcplusplus-$(LIBSIGCPLUSPLUS_VERSION).tar.gz

libsigcplusplus: libsigcplusplus-$(LIBSIGCPLUSPLUS_VERSION).tar.gz .sum-libsigcplusplus
	$(UNPACK)
	$(MOVE)

.libsigcplusplus: libsigcplusplus
	cd $< && $(HOSTVARS) NOCONFIGURE=1 ./autogen.sh
	cd $< && $(HOSTVARS) ./configure --disable-documentation --disable-deprecated-api $(HOSTCONF) && $(MAKE) && $(MAKE) install
	touch $@
