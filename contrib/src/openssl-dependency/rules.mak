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

OPENSSL_VERSION := a6406c95984a1009f5676bbcf60cc0d6db107af4
OPENSSL_URL := https://github.com/Corvusoft/openssl-dependency/archive/$(OPENSSL_VERSION).tar.gz

# Pure dependency of restbed: do not add to PKGS.

$(TARBALLS)/openssl-dependency-$(OPENSSL_VERSION).tar.gz:
	$(call download,$(OPENSSL_URL))

openssl-dependency: openssl-dependency-$(OPENSSL_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.openssl-dependency: openssl-dependency .sum-openssl-dependency
	mkdir -p "$(PREFIX)/include"
ifdef HAVE_WIN32
	cd $< && ./Configure --prefix=$(PREFIX) --libdir=lib --cross-compile-prefix=i686-w64-mingw32- mingw && make && make install -j 8
else ifdef HAVE_WIN64
	cd $< && ./Configure --prefix=$(PREFIX) --libdir=lib --cross-compile-prefix=x86_64-w64-mingw32- mingw && make && make install -j 8
else
	cd $< && ./config --prefix=$(PREFIX) --libdir=lib && make && make install -j 8
endif
	touch $@

.sum-openssl-dependency: openssl-dependency-$(OPENSSL_VERSION).tar.gz
