# HTTP_PARSER
HTTP_PARSER_VERSION := 2.9.4
HTTP_PARSER_URL := https://github.com/nodejs/http-parser/archive/v$(HTTP_PARSER_VERSION).tar.gz

PKGS += http-parser
ifeq ($(call need_pkg,'http-parser'),)
PKGS_FOUND += http-parser
endif

HTTP_PARSER_MAKECONF := PREFIX=$(PREFIX)

$(TARBALLS)/http-parser-$(HTTP_PARSER_VERSION).tar.gz:
	$(call download,$(HTTP_PARSER_URL))

.sum-http-parser: http-parser-$(HTTP_PARSER_VERSION).tar.gz

http-parser: http-parser-$(HTTP_PARSER_VERSION).tar.gz
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.http-parser: http-parser toolchain.cmake .sum-http-parser
	cd $< && $(HOSTVARS) $(MAKE) $(HTTP_PARSER_MAKECONF) package
	cd $< && cp -f http_parser.h $(PREFIX)/include && cp -f libhttp_parser.a $(PREFIX)/lib
	touch $@
