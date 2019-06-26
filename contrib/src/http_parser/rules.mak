# HTTP_PARSER
HTTP_PARSER_VERSION := 2.9.3
HTTP_PARSER_URL := https://github.com/binarytrails/http_parser/archive/v$(HTTP_PARSER_VERSION).tar.gz

PKGS += http_parser
ifeq ($(call need_pkg,'http_parser'),)
PKGS_FOUND += http_parser
endif

HTTP_PARSER_CMAKECONF := package

$(TARBALLS)/http_parser-$(HTTP_PARSER_VERSION).tar.gz:
	$(call download,$(HTTP_PARSER_URL))

.sum-http_parser: http_parser-$(HTTP_PARSER_VERSION).tar.gz

http_parser: http_parser-$(HTTP_PARSER_VERSION).tar.gz
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.http_parser: http_parser toolchain.cmake .sum-http_parser
	cd $< && $(HOSTVARS) $(MAKE) $(HTTP_PARSER_MAKECONF)
	cd $< && $(MAKE) install
	touch $@
