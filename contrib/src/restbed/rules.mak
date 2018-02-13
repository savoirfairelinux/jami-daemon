#
#  Copyright (C) 2016 Savoir-faire Linux Inc.
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

RESTBED_VERSION := df867a858dddc4cf6ca8642da02720bd65ba239a
RESTBED_URL := https://github.com/corvusoft//restbed/archive/$(RESTBED_VERSION).tar.gz

PKGS += restbed

ifeq ($(call need_pkg,"restbed >= 4.0"),)
PKGS_FOUND += restbed
endif

$(TARBALLS)/restbed-$(RESTBED_VERSION).tar.gz:
	$(call download,$(RESTBED_URL))

DEPS_restbed = asio kashmir-dependency

RESTBED_CONF = -DBUILD_TESTS=NO \
			-DBUILD_EXAMPLES=NO \
			-DBUILD_SSL=NO \
			-DBUILD_SHARED=NO \
			-DCMAKE_INCLUDE_PATH=$(PREFIX)/include \
			-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
			-DCMAKE_INSTALL_LIBDIR=lib

restbed: restbed-$(RESTBED_VERSION).tar.gz .sum-restbed
	$(UNPACK)
	$(APPLY) $(SRC)/restbed/strand.patch
	$(APPLY) $(SRC)/restbed/async_read_until.patch
	$(MOVE)

.restbed: restbed toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) $(RESTBED_CONF) .
	cd $< && $(MAKE) install
	touch $@

.sum-restbed: restbed-$(RESTBED_VERSION).tar.gz
