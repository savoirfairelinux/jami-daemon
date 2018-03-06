# -*- mode: makefile; -*-
#
#  Copyright (C) 2018 Savoir-faire Linux Inc.
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

LIBRESSL_VERSION := 190bd346e75575b9436a2e9e14b28618f0234e1b
LIBRESSL_URL := https://github.com/libressl-portable/portable/archive/$(LIBRESSL_VERSION).tar.gz


# Pure dependency of restbed: do not add to PKGS.

$(TARBALLS)/portable-$(LIBRESSL_VERSION).tar.gz:
	$(call download,$(LIBRESSL_URL))

libressl: portable-$(LIBRESSL_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) $(SRC)/libressl/getpagesize.patch
	$(MOVE)

.libressl: libressl .sum-libressl
	mkdir -p "$(PREFIX)/include"
	cd $< && ./autogen.sh
ifdef HAVE_WIN32
	cd $< && CC=i686-w64-mingw32-gcc CPPFLAGS=-D__MINGW_USE_VC2005_COMPAT ./configure --host=i686-w64-mingw32 && $(MAKE) && DESTDIR=$(PREFIX) $(MAKE) install
else ifdef HAVE_WIN64
	cd $< && CC=x86_64-w64-mingw32-gcc ./configure --host=x86_64-w64-mingw32 && $(MAKE) && DESTDIR=$(PREFIX) $(MAKE) install
else ifeq ($(IOS_TARGET_PLATFORM),iPhoneOS)
	cd $< && mkdir build && cd build && $(CMAKE) -DDESTDIR=$(PREFIX) -DCMAKE_C_FLAGS='-miphoneos-version-min=9.3 -arch arm64' .. && $(MAKE) && $(MAKE) install
else ifeq ($(IOS_TARGET_PLATFORM),iPhoneSimulator)
	cd $< && mkdir build && cd build && $(CMAKE) -DDESTDIR=$(PREFIX) -DCMAKE_C_FLAGS='-miphoneos-version-min=9.3 -arch x86_64' .. && $(MAKE) && $(MAKE) install
else
	cd $< && mkdir build && cd build && $(CMAKE) -DDESTDIR=$(PREFIX) .. && $(MAKE) && $(MAKE) install
endif
	rm -rf $(PREFIX)/lib/*.so $(PREFIX)/lib/*.so.*
	touch $@

.sum-libressl: portable-$(LIBRESSL_VERSION).tar.gz
