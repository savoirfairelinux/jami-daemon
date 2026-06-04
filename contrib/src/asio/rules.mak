ASIO_VERSION := bd500f0a018db9a845ebaaed5c0318343ae9f497
PKG_CPE += cpe:2.3:a:*:asio:1.38.0:*:*:*:*:*:*:*
ASIO_URL := https://github.com/chriskohlhoff/asio/archive/$(ASIO_VERSION).tar.gz

ifeq ($(call need_pkg,'asio >= 1.30'),)
PKGS_FOUND += asio
endif

$(TARBALLS)/asio-$(ASIO_VERSION).tar.gz:
	$(call download,$(ASIO_URL))

asio: asio-$(ASIO_VERSION).tar.gz
	$(UNPACK)
	rm -rf asio-$(ASIO_VERSION)/asio
	$(APPLY) $(SRC)/asio/0001-Disable-building-tests-and-examples.patch
	$(MOVE)

.asio: asio .sum-asio
	cd $< && ./autogen.sh
	cd $< && $(HOSTVARS) ./configure --without-boost $(HOSTCONF)
	cd $< && $(MAKE) install
	mkdir -p $(PREFIX)/lib/pkgconfig
	mv $(PREFIX)/share/pkgconfig/asio.pc $(PREFIX)/lib/pkgconfig/asio.pc
	touch $@

.sum-asio: asio-$(ASIO_VERSION).tar.gz
