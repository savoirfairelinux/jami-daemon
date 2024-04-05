# -*- mode: makefile; -*-
#
#  Copyright (C) 2018-2024 Savoir-faire Linux Inc.
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
LLHTTP_VERSION := 9.2.0
LLHTTP_URL := https://github.com/nodejs/llhttp/archive/refs/tags/release/v$(LLHTTP_VERSION).tar.gz

LLHTTP_CMAKECONF := \
	-DBUILD_SHARED_LIBS=Off \
	-DBUILD_STATIC_LIBS=On

$(TARBALLS)/llhttp-$(LLHTTP_VERSION).tar.gz:
	$(call download,$(LLHTTP_URL))

llhttp: llhttp-$(LLHTTP_VERSION).tar.gz
	$(UNPACK)
	mv llhttp-release-v$(LLHTTP_VERSION) $@

.llhttp: llhttp .sum-llhttp
	cd $< && mkdir -p build && cd build && $(CMAKE) $(LLHTTP_CMAKECONF) ..
	cd $</build && $(MAKE) && $(MAKE) install
	touch $@

.sum-llhttp: llhttp-$(LLHTTP_VERSION).tar.gz
