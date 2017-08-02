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

RESTBED_VERSION := 4.6
RESTBED_URL := https://github.com/Corvusoft/restbed/archive/$(RESTBED_VERSION).tar.gz

PKGS += restbed

ifeq ($(call need_pkg,"restbed >= 4.0"),)
PKGS_FOUND += restbed
endif

$(TARBALLS)/restbed-$(RESTBED_VERSION).tar.gz:
	$(call download,$(RESTBED_URL))

DEPS_restbed = asio

RESTBED_CONF = -DBUILD_TESTS=NO \
			-DBUILD_EXAMPLES=NO \
			-DBUILD_SSL=NO \
			-DBUILD_SHARED=NO \
			-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
			-DCMAKE_INSTALL_LIBDIR=lib

restbed: restbed-$(RESTBED_VERSION).tar.gz .sum-restbed
	$(UNPACK)
	(cd $(UNPACK_DIR)/dependency && \
	curl -L https://github.com/Corvusoft/kashmir-dependency/archive/master.tar.gz | tar xvz && \
	rm -r kashmir && mv kashmir-dependency-master kashmir)
	$(APPLY) $(SRC)/restbed/findkashmir.patch
	$(APPLY) $(SRC)/restbed/strand.patch
	$(APPLY) $(SRC)/restbed/locale-fix.patch
	$(MOVE)

.restbed: restbed toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) $(RESTBED_CONF) .
	cd $< && $(MAKE) install
	touch $@

.sum-restbed: restbed-$(RESTBED_VERSION).tar.gz
