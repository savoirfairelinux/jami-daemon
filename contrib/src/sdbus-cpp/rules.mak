SDBUS_CPP_VERSION := 2.1.0
SDBUS_CPP_URL := https://github.com/Kistler-Group/sdbus-cpp/archive/refs/tags/v$(SDBUS_CPP_VERSION).tar.gz

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += sdbus-cpp
endif
endif

ifeq ($(call need_pkg,"sdbus-c++ >= 2.0.0"),)
PKGS_FOUND += sdbus-cpp
endif

SDBUS_CPP_CMAKECONF := -D CMAKE_BUILD_TYPE=Release \
                       -D BUILD_SHARED_LIBS=OFF \
                       -D SDBUSCPP_BUILD_CODEGEN=ON \
                       -D SDBUSCPP_BUILD_DOCS=OFF

$(TARBALLS)/sdbus-cpp-$(SDBUS_CPP_VERSION).tar.gz:
	$(call download,$(SDBUS_CPP_URL))

.sum-sdbus-cpp: sdbus-cpp-$(SDBUS_CPP_VERSION).tar.gz

sdbus-cpp: sdbus-cpp-$(SDBUS_CPP_VERSION).tar.gz .sum-sdbus-cpp
	$(UNPACK)
	$(MOVE)

.sdbus-cpp: sdbus-cpp
	cd $< && $(HOSTVARS) $(CMAKE) $(SDBUS_CPP_CMAKECONF) .
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@
