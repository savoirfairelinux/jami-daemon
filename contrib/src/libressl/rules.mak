# -*- mode: makefile; -*-
#
#  Copyright (C) 2018-2023 Savoir-faire Linux Inc.
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
SSL_VERSION := 3.7.0
LIBRESSL_VERSION := libressl-$(SSL_VERSION)
LIBRESSL_URL := https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/$(LIBRESSL_VERSION).tar.gz

# Check if openssl or libressl is already present on the system
ifeq ($(or $(call need_pkg,"openssl >= 1.0.0"),$(call need_pkg,"libressl >= 1.0.0")),)
PKGS_FOUND += libressl
endif

$(TARBALLS)/$(LIBRESSL_VERSION).tar.gz:
	$(call download,$(LIBRESSL_URL))

libressl: $(LIBRESSL_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

LIBRESSL_CONF := \
	-DLIBRESSL_TESTS=Off \
	-DLIBRESSL_APPS=Off  \
	-DDESTDIR=$(PREFIX)

ifdef HAVE_ANDROID
ifeq ($(ARCH),x86_64)
LIBRESSL_CONF += -DENABLE_ASM=Off
endif
else ifeq ($(IOS_TARGET_PLATFORM),iPhoneOS)
LIBRESSL_CONF += -DCMAKE_C_FLAGS='-miphoneos-version-min=9.3 -fembed-bitcode -arch arm64'
else ifeq ($(IOS_TARGET_PLATFORM),iPhoneSimulator)
LIBRESSL_CONF += -DCMAKE_C_FLAGS='-miphoneos-version-min=9.3 -fembed-bitcode -arch x86_64'
else ifeq ($(HOST_ARCH),arm-linux-gnueabihf)
LIBRESSL_CONF += -DCMAKE_SYSTEM_PROCESSOR=arm -DENABLE_ASM=Off -DCMAKE_C_FLAGS='-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard'
endif

.libressl: libressl .sum-libressl
	mkdir -p "$(PREFIX)/include"
ifdef HAVE_WIN32
	cd $< && $(HOSTVARS) CPPFLAGS=-D__MINGW_USE_VC2005_COMPAT ./configure $(HOSTCONF) && $(MAKE) && $(MAKE) install
else ifdef HAVE_WIN64
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) && $(MAKE) && $(MAKE) install
else
	cd $< && mkdir -p build && cd build && $(CMAKE) $(LIBRESSL_CONF) .. && $(MAKE) && $(MAKE) install
endif
	rm -rf $(PREFIX)/lib/*.so $(PREFIX)/lib/*.so.*
	touch $@

.sum-libressl: $(LIBRESSL_VERSION).tar.gz
