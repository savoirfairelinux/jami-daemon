# HTTP_PARSER
HTTP_PARSER_VERSION := 2.9.4
PKG_CPE += cpe:2.3:a:nodejs:http-parser:$(HTTP_PARSER_VERSION):*:*:*:*:*:*:*
HTTP_PARSER_URL := https://github.com/nodejs/http-parser/archive/v$(HTTP_PARSER_VERSION).tar.gz

PKGS += http_parser
ifeq ($(call need_pkg,'http_parser'),)
PKGS_FOUND += http_parser
endif

HTTP_PARSER_MAKECONF := PREFIX=$(PREFIX)

$(TARBALLS)/http-parser-$(HTTP_PARSER_VERSION).tar.gz:
	$(call download,$(HTTP_PARSER_URL))

.sum-http_parser: http-parser-$(HTTP_PARSER_VERSION).tar.gz

http_parser: http-parser-$(HTTP_PARSER_VERSION).tar.gz
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.http_parser: http_parser toolchain.cmake .sum-http_parser
	cd $< && $(HOSTVARS) $(MAKE) $(HTTP_PARSER_MAKECONF) package
	mkdir -p $(PREFIX)/include
	mkdir -p $(PREFIX)/lib
	cd $< && cp -f http_parser.h $(PREFIX)/include && cp -f libhttp_parser.a $(PREFIX)/lib
	touch $@
