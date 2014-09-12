# YAML
YAML_CPP_VERSION := 0.5.1
YAML_CPP_URL := http://yaml-cpp.googlecode.com/files/yaml-cpp-$(YAML_CPP_VERSION).tar.gz

PKGS += yaml-cpp

ifeq ($(call need_pkg,'yaml-cpp'),)
PKGS_FOUND += yaml-cpp
endif

DEPS_yaml-cpp = boost-headers $(DEPS_boost-headers)

YAML_CPP_CMAKECONF := -DBUILD_STATIC:BOOL=ON \
                      -DBUILD_SHARED:BOOL=OFF \
                      -DBoost_INCLUDE_DIR=../../$(HOST)/include \
                      -DBUILD_SHARED_LIBS:BOOL=OFF

$(TARBALLS)/yaml-cpp-$(YAML_CPP_VERSION).tar.gz:
	$(call download,$(YAML_CPP_URL))

.sum-yaml-cpp: yaml-cpp-$(YAML_CPP_VERSION).tar.gz

yaml-cpp: yaml-cpp-$(YAML_CPP_VERSION).tar.gz .sum-yaml-cpp
	$(UNPACK)
	$(MOVE)

.yaml-cpp: yaml-cpp toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) . $(YAML_CPP_CMAKECONF)
	cd $< && $(MAKE) install
	touch $@
