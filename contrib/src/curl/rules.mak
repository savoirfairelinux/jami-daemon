# CURL
CURL_VERSION := 7.66.0
CURL_URL := https://curl.haxx.se/download/curl-${CURL_VERSION}.tar.gz

PKGS += curl
ifeq ($(call need_pkg,"curl >= 7.66.0"),)
PKGS_FOUND += curl
endif

$(TARBALLS)/curl-$(CURL_VERSION).tar.gz:
	$(call download,$(CURL_URL))

.sum-curl: curl-$(CURL_VERSION).tar.gz

curl: curl-$(CURL_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.curl: curl
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
