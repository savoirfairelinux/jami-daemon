#
#  Copyright (C) 2016 Savoir-faire Linux Inc.
#
#  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

ASIO_VERSION := 722f7e2be05a51c69644662ec514d6149b2b7ef8
ASIO_URL := https://github.com/Corvusoft/asio-dependency/archive/$(ASIO_VERSION).tar.gz

PKGS += asio

$(TARBALLS)/asio-$(ASIO_VERSION).tar.gz:
	$(call download,$(ASIO_URL))

asio: asio-$(ASIO_VERSION).tar.gz
	$(UNPACK)
	mv asio-dependency-$(ASIO_VERSION)/asio asio-$(ASIO_VERSION) && rm -rf asio-dependency-$(ASIO_VERSION)
	$(APPLY) $(SRC)/asio/conditional_sslv3.patch
	$(MOVE)

.asio: asio .sum-asio
	cd $< && ./autogen.sh
	cd $< && $(HOSTVARS) ./configure --without-boost $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@

.sum-asio: asio-$(ASIO_VERSION).tar.gz
