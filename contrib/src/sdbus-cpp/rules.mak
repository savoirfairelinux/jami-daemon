SDBUS_CPP_VERSION := 0.7.2
SDBUS_CPP_URL := https://github.com/Kistler-Group/sdbus-cpp/archive/v$(SDBUS_CPP_VERSION).tar.gz

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += sdbus-cpp
endif
endif

ifeq ($(call need_pkg,"sdbus-c++ >= 0.7.2"),)
PKGS_FOUND += sdbus-cpp
endif

DEPS_sdbus-cpp += expat

SDBUS_CPP_CMAKECONF := -DCMAKE_BUILD_TYPE=release \
                       -DBUILD_SHARED_LIBS=OFF \
                       -DBUILD_CODE_GEN=ON \
                       -DBUILD_LIBSYSTEMD=ON \
                       -DBUILD_TESTS=OFF \
                       -DBUILD_DOC=OFF

$(TARBALLS)/sdbus-cpp-${SDBUS_CPP_VERSION}.tar.gz:
	$(call download,$(SDBUS_CPP_URL))

.sum-sdbus-cpp: sdbus-cpp-${SDBUS_CPP_VERSION}.tar.gz

sdbus-cpp: sdbus-cpp-${SDBUS_CPP_VERSION}.tar.gz .sum-sdbus-cpp
	$(UNPACK)
	$(MOVE)

.sdbus-cpp: sdbus-cpp
	cd $< && $(HOSTVARS) $(CMAKE) $(SDBUS_CPP_CMAKECONF) .
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@
