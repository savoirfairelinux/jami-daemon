# YAML
YAML_CPP_VERSION := yaml-cpp-0.9.0
PKG_CPE += cpe:2.3:a:*:yaml-cpp:$(YAML_CPP_VERSION):*:*:*:*:*:*:*
YAML_CPP_URL := https://github.com/jbeder/yaml-cpp/archive/$(YAML_CPP_VERSION).tar.gz

PKGS += yaml-cpp

ifeq ($(call need_pkg,'yaml-cpp >= 0.8.0'),)
PKGS_FOUND += yaml-cpp
endif

YAML_CPP_CONF := -DCMAKE_CXX_STANDARD=20 \
				-DYAML_BUILD_SHARED_LIBS=OFF \
				-DYAML_CPP_BUILD_TOOLS=OFF \
				-DYAML_CPP_BUILD_TESTS=OFF \
				-DYAML_CPP_BUILD_CONTRIB=OFF \
				-DYAML_CPP_FORMAT_SOURCE=OFF

$(TARBALLS)/yaml-cpp-$(YAML_CPP_VERSION).tar.gz:
	$(call download,$(YAML_CPP_URL))

.sum-yaml-cpp: yaml-cpp-$(YAML_CPP_VERSION).tar.gz

yaml-cpp: yaml-cpp-$(YAML_CPP_VERSION).tar.gz .sum-yaml-cpp
	$(UNPACK)
	$(MOVE)

CMAKE_PKGS += yaml-cpp
