#
#  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
#
#  Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
#          Adrien Béraud <adrien.beraud@savoirfairelinux.com
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

RESTBED_VERSION := 58eaf0a1df49917145357f86c87b3f1acadaa66a
RESTBED_URL := https://github.com/aberaud/restbed/archive/$(RESTBED_VERSION).tar.gz

# Pure dependency of OpenDHT: do not add to PKGS.

ifeq ($(call need_pkg,"restbed >= 4.0"),)
PKGS_FOUND += restbed
endif

$(TARBALLS)/restbed-$(RESTBED_VERSION).tar.gz:
	$(call download,$(RESTBED_URL))

DEPS_restbed = asio libressl

RESTBED_CONF = -DBUILD_TESTS=NO \
			-DBUILD_SSL=YES \
			-DBUILD_STATIC=YES \
			-DBUILD_SHARED=NO \
			-DCMAKE_INCLUDE_PATH=$(PREFIX)/include \
			-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
			-DCMAKE_INSTALL_LIBDIR=lib

restbed: restbed-$(RESTBED_VERSION).tar.gz .sum-restbed
	$(UNPACK)
	$(MOVE)

.restbed: restbed toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) $(RESTBED_CONF) .
	cd $< && $(MAKE) install
	touch $@

.sum-restbed: restbed-$(RESTBED_VERSION).tar.gz
