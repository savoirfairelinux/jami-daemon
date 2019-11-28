# YAML
YAML_CPP_VERSION := yaml-cpp-0.6.3
YAML_CPP_URL := https://github.com/jbeder/yaml-cpp/archive/$(YAML_CPP_VERSION).tar.gz

PKGS += yaml-cpp

ifeq ($(call need_pkg,'yaml-cpp >= 0.5.3'),)
PKGS_FOUND += yaml-cpp
endif

YAML_CPP_CMAKECONF := -DBUILD_STATIC=ON \
                      -DBUILD_SHARED=OFF \
                      -DYAML_CPP_BUILD_TOOLS=OFF \
                      -DYAML_CPP_BUILD_TESTS=OFF \
                      -DYAML_CPP_BUILD_CONTRIB=OFF \
                      -DBUILD_SHARED_LIBS=OFF

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
