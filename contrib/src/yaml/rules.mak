# YAML
YAML_VERSION := 0.1.5
YAML_URL := http://pyyaml.org/download/libyaml/yaml-$(YAML_VERSION).tar.gz

PKGS += yaml

$(TARBALLS)/yaml-$(YAML_VERSION).tar.gz:
	$(call download,$(YAML_URL))

.sum-yaml: yaml-$(YAML_VERSION).tar.gz

yaml: yaml-$(YAML_VERSION).tar.gz .sum-yaml
	$(UNPACK)
	$(MOVE)

.yaml: yaml
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
