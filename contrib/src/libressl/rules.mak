# -*- mode: makefile; -*-
#
#  Copyright (C) 2018-2025 Savoir-faire Linux Inc.
#
#  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <https://www.gnu.org/licenses/>.
#
SSL_VERSION := 4.1.0
PKG_CPE += cpe:2.3:a:openbsd:libressl:$(SSL_VERSION):*:*:*:*:*:*:*
LIBRESSL_VERSION := libressl-$(SSL_VERSION)
LIBRESSL_URL := https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/$(LIBRESSL_VERSION).tar.gz

# Check if openssl or libressl is already present on the system
ifeq ($(filter $(call need_pkg,"openssl >= 1.0.0"), $(call need_pkg,"libressl >= 1.0.0")),)
PKGS_FOUND += libressl
endif

$(TARBALLS)/$(LIBRESSL_VERSION).tar.gz:
	$(call download,$(LIBRESSL_URL))

libressl: $(LIBRESSL_VERSION).tar.gz
	$(UNPACK)
ifdef HAVE_IOS
	$(APPLY) $(SRC)/libressl/ios-add-byte-order-macros.patch
endif
	$(MOVE)

LIBRESSL_CONF := \
	-DBUILD_SHARED_LIBS=Off \
	-DLIBRESSL_TESTS=Off \
	-DLIBRESSL_APPS=Off

.libressl: libressl .sum-libressl
	mkdir -p "$(PREFIX)/include"
ifdef HAVE_WIN32
	cd $< && $(HOSTVARS) CPPFLAGS=-D__MINGW_USE_VC2005_COMPAT ./configure $(HOSTCONF) && $(MAKE) && $(MAKE) install
else ifdef HAVE_WIN64
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) && $(MAKE) && $(MAKE) install
else
	cd $< && mkdir -p build && cd build && $(HOSTVARS) $(CMAKE) $(LIBRESSL_CONF) .. && $(MAKE) && $(MAKE) install
endif
	rm -rf $(PREFIX)/lib/*.so $(PREFIX)/lib/*.so.*
	touch $@

.sum-libressl: $(LIBRESSL_VERSION).tar.gz
