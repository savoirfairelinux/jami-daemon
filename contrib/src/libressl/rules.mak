# -*- mode: makefile; -*-
#
#  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
#
#  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
#

LIBRESSL_VERSION := 0974145a9e62d844b5b159cddbce36552bca30df
OPENBSD_VERSION := OPENBSD_6_3
LIBRESSL_URL := https://github.com/libressl-portable/portable/archive/$(LIBRESSL_VERSION).tar.gz
OPENBSD_URL := https://github.com/libressl-portable/openbsd/archive/$(OPENBSD_VERSION).tar.gz

# Check if openssl or libressl is already present on the system
ifeq ($(call need_pkg,"openssl >= 1.0.0" || call need_pkg,"libressl >= 1.0.0"),)
PKGS_FOUND += libressl
endif

# Pure dependency of restinio: do not add to PKGS.

$(TARBALLS)/portable-$(LIBRESSL_VERSION).tar.gz:
	$(call download,$(LIBRESSL_URL))

$(TARBALLS)/openbsd-$(OPENBSD_VERSION).tar.gz:
	$(call download,$(OPENBSD_URL))

libressl: portable-$(LIBRESSL_VERSION).tar.gz openbsd-$(OPENBSD_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) $(SRC)/libressl/getpagesize.patch
	$(APPLY) $(SRC)/libressl/0001-build-don-t-fetch-git-tag-if-openbsd-directory-exist.patch
	mv openbsd-$(OPENBSD_VERSION) $(UNPACK_DIR)/openbsd
	$(MOVE)

.libressl: libressl .sum-libressl
	mkdir -p "$(PREFIX)/include"
	cd $< && ./autogen.sh
ifdef HAVE_WIN32
	cd $< && $(HOSTVARS) CPPFLAGS=-D__MINGW_USE_VC2005_COMPAT ./configure $(HOSTCONF) && $(MAKE) && $(MAKE) install
else ifdef HAVE_WIN64
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) && $(MAKE) && $(MAKE) install
else ifeq ($(IOS_TARGET_PLATFORM),iPhoneOS)
	cd $< && mkdir build && cd build && $(CMAKE) -DLIBRESSL_TESTS=Off -DLIBRESSL_APPS=Off -DDESTDIR=$(PREFIX) -DCMAKE_C_FLAGS='-miphoneos-version-min=9.3 -fembed-bitcode -arch arm64' .. && $(MAKE) && $(MAKE) install
else ifeq ($(IOS_TARGET_PLATFORM),iPhoneSimulator)
	cd $< && mkdir build && cd build && $(CMAKE) -DLIBRESSL_TESTS=Off -DLIBRESSL_APPS=Off -DDESTDIR=$(PREFIX) -DCMAKE_C_FLAGS='-miphoneos-version-min=9.3 -fembed-bitcode -arch x86_64' .. && $(MAKE) && $(MAKE) install
else
	cd $< && mkdir build && cd build && $(CMAKE) -DLIBRESSL_TESTS=Off -DLIBRESSL_APPS=Off -DDESTDIR=$(PREFIX) .. && $(MAKE) && $(MAKE) install
endif
	rm -rf $(PREFIX)/lib/*.so $(PREFIX)/lib/*.so.*
	touch $@

.sum-libressl: portable-$(LIBRESSL_VERSION).tar.gz
