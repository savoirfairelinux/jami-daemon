#
#  Copyright (C) 2016-2025 Savoir-faire Linux Inc.
#
#  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <https://www.gnu.org/licenses/>.
#

ASIO_VERSION := asio-1-34-2
PKG_CPE += cpe:2.3:a:*:asio:1.34.2:*:*:*:*:*:*:*
ASIO_URL := https://github.com/chriskohlhoff/asio/archive/$(ASIO_VERSION).tar.gz

# Pure dependency of restinio: do not add to PKGS.

$(TARBALLS)/asio-$(ASIO_VERSION).tar.gz:
	$(call download,$(ASIO_URL))

asio: asio-$(ASIO_VERSION).tar.gz
	$(UNPACK)
	mv asio-$(ASIO_VERSION)/asio/* asio-$(ASIO_VERSION)/ && rm -rf asio-$(ASIO_VERSION)/asio
	$(APPLY) $(SRC)/asio/0001-Disable-building-tests-and-examples.patch
	$(MOVE)

.asio: asio .sum-asio
	cd $< && ./autogen.sh
	cd $< && $(HOSTVARS) ./configure --without-boost $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@

.sum-asio: asio-$(ASIO_VERSION).tar.gz
