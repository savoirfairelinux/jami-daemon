# SIMDUTF
SIMDUTF_VERSION := 7.5.0
SIMDUTF_URL := https://github.com/simdutf/simdutf/archive/v$(SIMDUTF_VERSION).tar.gz

PKGS += simdutf
ifeq ($(call need_pkg,'simdutf >= 7.5.0'),)
PKGS_FOUND += simdutf
endif

SIMDUTF_CONF = -DBUILD_SHARED_LIBS=Off \
	-DBUILD_TESTING=Off \
	-DSIMDUTF_TESTS=Off \
	-DSIMDUTF_TOOLS=Off \
	-DSIMDUTF_CXX_STANDARD=17

$(TARBALLS)/simdutf-$(SIMDUTF_VERSION).tar.gz:
	$(call download,$(SIMDUTF_URL))

.sum-simdutf: simdutf-$(SIMDUTF_VERSION).tar.gz

simdutf: simdutf-$(SIMDUTF_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.simdutf: simdutf toolchain.cmake .sum-simdutf
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) $(SIMDUTF_CONF) ..
	cd $< && cd build && $(MAKE) install
	touch $@
