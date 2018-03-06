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

BORINGSSL_VERSION := 40cdb3b5dad0a1028e40fbc73fa62a3e6d31fed7
BORINGSSL_URL := https://github.com/google/boringssl/archive/$(BORINGSSL_VERSION).tar.gz

# Pure dependency of restbed: do not add to PKGS.

$(TARBALLS)/boringssl-$(BORINGSSL_VERSION).tar.gz:
	$(call download,$(BORINGSSL_URL))

boringssl: boringssl-$(BORINGSSL_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.boringssl: boringssl .sum-boringssl
	mkdir -p "$(PREFIX)/include"
	mkdir -p "$(PREFIX)/lib"
	cd $< && mkdir build && cd build && $(CMAKE) -DCMAKE_BUILD_TYPE=Release .. && make -j 8
	cd $< && cp -r include/openssl $(PREFIX)/include && cd build && cp ssl/libssl.a $(PREFIX)/lib/  && cp crypto/libcrypto.a $(PREFIX)/lib/
	touch $@

.sum-boringssl: boringssl-$(BORINGSSL_VERSION).tar.gz
