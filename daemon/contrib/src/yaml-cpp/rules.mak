# YAML
YAML_CPP_VERSION := 0.3.0
YAML_CPP_URL := http://yaml-cpp.googlecode.com/files/yaml-cpp-$(YAML_CPP_VERSION).tar.gz

PKGS += yaml-cpp

ifeq ($(call need_pkg,'yaml-cpp'),)
PKGS_FOUND += yaml-cpp
endif

YAML_CPP_CMAKECONF := -DBUILD_STATIC:BOOL=ON \
                      -DBUILD_SHARED:BOOL=OFF \
                      -DYAML_CPP_BUILD_TOOLS:BOOL=OFF \
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
