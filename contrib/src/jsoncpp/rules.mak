# JSONCPP
JSONCPP_VERSION := 1.9.6
PKG_CPE += cpe:2.3:a:jsoncpp_project:jsoncpp:$(JSONCPP_VERSION):*:*:*:*:*:*:*

JSONCPP_URL := https://github.com/open-source-parsers/jsoncpp/archive/$(JSONCPP_VERSION).tar.gz

PKGS += jsoncpp

ifeq ($(call need_pkg,"jsoncpp >= 1.9.3"),)
PKGS_FOUND += jsoncpp
endif

JSONCPP_CMAKECONF := -DBUILD_STATIC_LIBS:BOOL=ON \
                     -DBUILD_SHARED_LIBS:BOOL=OFF \
                     -DJSONCPP_WITH_TESTS:BOOL=OFF

$(TARBALLS)/jsoncpp-$(JSONCPP_VERSION).tar.gz:
	$(call download,$(JSONCPP_URL))

.sum-jsoncpp: jsoncpp-$(JSONCPP_VERSION).tar.gz

jsoncpp: jsoncpp-$(JSONCPP_VERSION).tar.gz .sum-jsoncpp
	$(UNPACK)
	$(MOVE)

.jsoncpp: jsoncpp toolchain.cmake
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) .. ${JSONCPP_CMAKECONF}
	cd $< && cd build && $(MAKE) install
	touch $@
