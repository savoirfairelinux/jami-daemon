# RESTINIO
RESTINIO_VERSION := v.0.5.1
RESTINIO_URL := https://github.com/Stiffstream/restinio/archive/$(RESTINIO_VERSION).tar.gz

PKGS += restinio
ifeq ($(call need_pkg,'restinio'),)
PKGS_FOUND += restinio
endif

# Avoid building distro-provided dependencies in case RESTinio was built manually
ifneq ($(call need_pkg,"fmt == 5.3.0"),)
DEPS_restinio += fmt
endif
ifneq ($(call need_pkg,"zlib"),)
DEPS_restinio += zlib
endif
ifneq ($(call need_pkg,"asio == asio-1-12-2"),)
DEPS_restinio += asio
endif
ifneq ($(call need_pkg,"http_parser == 2.9.3"),)
DEPS_restinio += http_parser
endif

RESTINIO_CMAKECONF = -DRESTINIO_TEST=OFF -DRESTINIO_SAMPLE=OFF -DRESTINIO_INSTALL_SAMPLES=OFF \
					 -DRESTINIO_BENCH=OFF -DRESTINIO_INSTALL_BENCHES=OFF \
					 -DRESTINIO_FIND_DEPS=ON -DRESTINIO_USE_BOOST_ASIO=none \
					 -DZLIB_LIBRARY="$(PREFIX)/lib" -DZLIB_INCLUDE_DIR="$(PREFIX)/include"

$(TARBALLS)/restinio-$(RESTINIO_VERSION).tar.gz:
	$(call download,$(RESTINIO_URL))

.sum-restinio: restinio-$(RESTINIO_VERSION).tar.gz

restinio: restinio-$(RESTINIO_VERSION).tar.gz
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.restinio: restinio .sum-restinio
	# remove unwanted & not-optional deps
	cd $</dev && sed -i.bak '/find_package(Boost*/d' CMakeLists.txt
	cd $</dev && sed -i.bak '/find_package(PCRE)/d' CMakeLists.txt
	cd $</dev && sed -i.bak '/find_package(PCRE2)/d' CMakeLists.txt
	cd $</dev && sed -i.bak '/add_subdirectory(so_5)/d' CMakeLists.txt
	# proceed with install
	cd $</dev && $(HOSTVARS) $(CMAKE) $(RESTINIO_CMAKECONF) .
	cd $</dev && $(MAKE) install
	touch $@
