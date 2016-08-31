#
#  Copyright (C) 2016 Savoir-faire Linux Inc.
#
#  Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
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

RESTBED_VERSION := 4.0
RESTBED_URL := https://github.com/Corvusoft/restbed/archive/$(RESTBED_VERSION).tar.gz

ifeq ($(call need_pkg,"restbed >= 4.0"),)
PKGS_FOUND += restbed
endif

$(TARBALLS)/restbed-$(RESTBED_VERSION).tar.gz:
	$(call download,$(RESTBED_URL))

RESTBED_CONF = -DBUILD_TESTS=NO \
			-DBUILD_EXAMPLES=NO \
			-DBUILD_SSL=NO \
			-DBUILD_SHARED=NO \
			-DCMAKE_C_COMPILER=gcc \
			-DCMAKE_CXX_COMPILER=g++ \
			-DCMAKE_INSTALL_PREFIX=$(PREFIX)

restbed: restbed-$(RESTBED_VERSION).tar.gz
	$(UNPACK)
	git clone https://github.com/Corvusoft/asio-dependency.git $(UNPACK_DIR)/dependency/asio/
	cd $(UNPACK_DIR)/dependency/asio/asio && patch -fp1 < ../../../../../src/restbed/conditional_sslv3.patch
	cd $(UNPACK_DIR)/ && ls && patch -p0 < ../../src/restbed/CMakeLists.patch
	$(MOVE)

.restbed: restbed
	cd $< && cmake $(RESTBED_CONF) .
	cd $< && $(MAKE) install
	touch $@

.sum-restbed: restbed-$(RESTBED_VERSION).tar.gz