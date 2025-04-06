# YAML
YAML_CPP_VERSION := 28f93bdec6387d42332220afa9558060c8016795
PKG_CPE += cpe:2.3:a:*:yaml-cpp:$(YAML_CPP_VERSION):*:*:*:*:*:*:*
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
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) .. $(YAML_CPP_CMAKECONF)
	cd $< && cd build && $(MAKE) install
	touch $@
