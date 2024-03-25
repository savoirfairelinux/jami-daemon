# RESTINIO
RESTINIO_VERSION := 0.7.2
RESTINIO_URL := https://github.com/Stiffstream/restinio/releases/download/v.${RESTINIO_VERSION}/restinio-${RESTINIO_VERSION}.tar.bz2

PKGS += restinio
ifeq ($(call need_pkg,'restinio'),)
PKGS_FOUND += restinio
endif

# Avoid building distro-provided dependencies in case RESTinio was built manually
ifneq ($(call need_pkg,"zlib"),)
DEPS_restinio += zlib
endif
ifneq ($(call need_pkg,"asio"),)
DEPS_restinio += asio
endif
ifneq ($(call need_pkg,"fmt >= 5.3.0"),)
DEPS_restinio += fmt
endif
DEPS_restinio += http_parser

RESTINIO_CMAKECONF = -DRESTINIO_TEST=Off -DRESTINIO_SAMPLE=Off -DRESTINIO_BENCHMARK=Off \
                -DRESTINIO_WITH_SOBJECTIZER=Off -DRESTINIO_DEP_STANDALONE_ASIO=system -DRESTINIO_DEP_LLHTTP=system \
                -DRESTINIO_DEP_FMT=system -DRESTINIO_DEP_EXPECTED_LITE=system .

$(TARBALLS)/restinio-$(RESTINIO_VERSION).tar.bz2:
	$(call download,$(RESTINIO_URL))

.sum-restinio: #restinio-$(RESTINIO_VERSION).tar.bz2
	$(warning $@ not implemented)
	touch $@

restinio: restinio-$(RESTINIO_VERSION).tar.bz2
	$(UNPACK)
	#$(APPLY) $(SRC)/restinio/cmake.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.restinio: restinio .sum-restinio
	cd $</dev && $(HOSTVARS) $(CMAKE) $(RESTINIO_CMAKECONF) .
	cd $</dev && $(MAKE) install
	touch $@
