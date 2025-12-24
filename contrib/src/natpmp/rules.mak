# libnatpmp
NATPMP_VERSION := 0c56980a3dcfab08bd7dd145d49fb4868fbaf1ca
NATPMP_URL := https://github.com/miniupnp/libnatpmp/archive/${NATPMP_VERSION}.tar.gz

ifndef HAVE_WIN32
PKGS += natpmp
endif

ifeq ($(call need_pkg,'libnatpmp'),)
PKGS_FOUND += natpmp
endif

$(TARBALLS)/libnatpmp-$(NATPMP_VERSION).tar.gz:
	$(call download,$(NATPMP_URL))

.sum-natpmp: libnatpmp-$(NATPMP_VERSION).tar.gz

natpmp: libnatpmp-$(NATPMP_VERSION).tar.gz .sum-natpmp
	$(UNPACK)
ifdef HAVE_IOS
	$(APPLY) $(SRC)/natpmp/disable_sysctl_on_ios.patch
endif
	$(APPLY) $(SRC)/natpmp/0001-avoid-SIGPIPE-on-macOS-iOS.patch
	$(APPLY) $(SRC)/natpmp/0001-Add-NATPMP_BUILD_TOOLS-option-to-conditionally-build.patch
	$(MOVE)

NATPMP_CONF = -DNATPMP_BUILD_TOOLS=OFF

CMAKE_PKGS += natpmp
