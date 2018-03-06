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

OPENSSL_VERSION := a6406c95984a1009f5676bbcf60cc0d6db107af4
OPENSSL_URL := https://github.com/Corvusoft/openssl-dependency/archive/$(OPENSSL_VERSION).tar.gz

ifdef HAVE_CROSS_COMPILE
ifndef HAVE_IOS
OPENSSL_CROSS := $(CROSS_COMPILE)
endif
else
OPENSSL_CROSS :=
endif


ifeq ($(ARCH),arm)
OPENSSL_ARCH := armv7
else ifeq ($(ARCH),arm64)
OPENSSL_ARCH := arm64
else ifeq ($(ARCH),i386)
OPENSSL_ARCH := x86
else ifeq ($(ARCH),mips)
OPENSSL_ARCH := mips32
else ifeq ($(ARCH),ppc)
OPENSSL_ARCH := ppc32
else ifeq ($(ARCH),ppc64)
OPENSSL_ARCH := ppc64
else ifeq ($(ARCH),sparc)
OPENSSL_ARCH := sparc
else ifeq ($(ARCH),x86_64)
OPENSSL_ARCH := x86_64
endif

ifdef HAVE_ANDROID
OPENSSL_OS := android
else ifdef HAVE_LINUX
OPENSSL_OS := linux
else ifdef HAVE_DARWIN_OS
ifeq ($(IOS_TARGET_PLATFORM),iPhoneSimulator)
OPENSSL_OS := iphonesimulator
else ifeq ($(ARCH),armv7)
OPENSSL_OS := darwin
else ifeq ($(ARCH),arm64)
OPENSSL_OS := darwin
else
ifeq ($(OSX_VERSION),10.5)
OPENSSL_OS := darwin9
else
OPENSSL_OS := darwin10
endif
endif
else ifdef HAVE_SOLARIS
OPENSSL_OS := solaris
else ifdef HAVE_WIN64 # must be before WIN32
OPENSSL_OS := win64
else ifdef HAVE_WIN32
OPENSSL_OS := win32
else ifdef HAVE_BSD
OPENSSL_OS := linux
endif

OPENSSL_TARGET := generic-gnu
ifdef OPENSSL_ARCH
ifdef OPENSSL_OS
OPENSSL_TARGET := $(OPENSSL_ARCH)-$(OPENSSL_OS)-gcc
endif
endif

# Pure dependency of restbed: do not add to PKGS.

$(TARBALLS)/openssl-dependency-$(OPENSSL_VERSION).tar.gz:
	$(call download,$(OPENSSL_URL))

openssl-dependency: openssl-dependency-$(OPENSSL_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.openssl-dependency: openssl-dependency .sum-openssl-dependency
	mkdir -p "$(PREFIX)/include"
ifdef HAVE_WIN32
	cd $< && ./Configure --prefix=$(PREFIX) --libdir=lib --cross-compile-prefix=i686-w64-mingw32- mingw
else ifdef HAVE_WIN64
	cd $< && ./Configure --prefix=$(PREFIX) --libdir=lib --cross-compile-prefix=x86_64-w64-mingw32- mingw64
else ifdef HAVE_ANDROID
	cd $< && ./Configure --prefix=$(PREFIX) --libdir=lib --cross-compile-prefix=$(CROSS_COMPILE) android-$(OPENSSL_ARCH)
else
	cd $< && ./config --prefix=$(PREFIX) --libdir=lib && make && make install -j 8
endif
	cd $<  && make && make install -j 8
	touch $@

.sum-openssl-dependency: openssl-dependency-$(OPENSSL_VERSION).tar.gz
