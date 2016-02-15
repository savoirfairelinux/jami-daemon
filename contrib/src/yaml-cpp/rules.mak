# YAML
YAML_CPP_VERSION := 24fa1b33805c9a91df0f32c46c28e314dd7ad96f
YAML_CPP_URL := https://github.com/jbeder/yaml-cpp/archive/$(YAML_CPP_VERSION).tar.gz

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
	$(APPLY) $(SRC)/yaml-cpp/cmake.patch
	$(MOVE)

.yaml-cpp: yaml-cpp toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) . $(YAML_CPP_CMAKECONF)
	cd $< && $(MAKE) install
	touch $@
