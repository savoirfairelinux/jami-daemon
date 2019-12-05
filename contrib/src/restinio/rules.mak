# RESTINIO
RESTINIO_VERSION := a7a10e419d9089c5b8ee63f5e3098c892f22fae4
RESTINIO_URL := https://github.com/aberaud/restinio/archive/$(RESTINIO_VERSION).tar.gz

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

RESTINIO_CMAKECONF = -DRESTINIO_TEST=OFF -DRESTINIO_SAMPLE=OFF -DRESTINIO_INSTALL_SAMPLES=OFF \
					 -DRESTINIO_BENCH=OFF -DRESTINIO_INSTALL_BENCHES=OFF -DRESTINIO_FIND_DEPS=ON \
					 -DRESTINIO_ALLOW_SOBJECTIZER=OFF -DRESTINIO_USE_BOOST_ASIO=none \
					 -DZLIB_LIBRARY="$(PREFIX)/lib" -DZLIB_INCLUDE_DIR="$(PREFIX)/include"

$(TARBALLS)/restinio-$(RESTINIO_VERSION).tar.gz:
	$(call download,$(RESTINIO_URL))

.sum-restinio: restinio-$(RESTINIO_VERSION).tar.gz

restinio: restinio-$(RESTINIO_VERSION).tar.gz
	$(UNPACK)
	#$(APPLY) $(SRC)/restinio/cmake.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.restinio: restinio .sum-restinio
	cd $</dev && $(HOSTVARS) $(CMAKE) $(RESTINIO_CMAKECONF) .
	cd $</dev && $(MAKE) install
	touch $@
