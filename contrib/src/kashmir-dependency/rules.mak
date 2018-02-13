#
#  Copyright (C) 2018 Savoir-faire Linux Inc.
#
#  Author: Maxim Cournoyer <maxim.cournoyer@savoirfairelinux.com>
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

KASHMIR_VERSION := 2f3913f49c4ac7f9bff9224db5178f6f8f0ff3ee
KASHMIR_URL := https://github.com/Corvusoft/kashmir-dependency/archive/$(KASHMIR_VERSION).tar.gz

PKGS += kashmir-dependency

$(TARBALLS)/kashmir-dependency-$(KASHMIR_VERSION).tar.gz:
	$(call download,$(KASHMIR_URL))

kashmir-dependency: kashmir-dependency-$(KASHMIR_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.kashmir-dependency: kashmir-dependency .sum-kashmir-dependency
	cd $< && cp -r kashmir $(PREFIX)/include
	touch $@

.sum-kashmir-dependency: kashmir-dependency-$(KASHMIR_VERSION).tar.gz
