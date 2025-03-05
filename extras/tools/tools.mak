# Copyright (C) 2003-2011 the VideoLAN team
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
#

include packages.mak

#
# common rules
#

AUTOCONF=$(PREFIX)/bin/autoconf
export AUTOCONF

# Set following to non-empty to remove uneeded progression reports (i.e. with automatics builds)
BATCH_MODE = 1

ifeq ($(shell curl --version >/dev/null 2>&1 || echo FAIL),)
download = curl $(if ${BATCH_MODE},-sS) -f -L --retry-delay 10 --retry 2 -- "$(1)" > "$@"
else ifeq ($(shell wget --version >/dev/null 2>&1 || echo FAIL),)
download = rm -f $@.tmp && \
	wget $(if ${BATCH_MODE},-nv) --passive -t 2 -w 10 -c -p -O $@.tmp "$(1)" && \
	touch $@.tmp && \
	mv $@.tmp $@
else ifeq ($(which fetch >/dev/null 2>&1 || echo FAIL),)
download = rm -f $@.tmp && \
	fetch -p -o $@.tmp "$(1)" && \
	touch $@.tmp && \
	mv $@.tmp $@
else
download = $(error Neither curl nor wget found!)
endif

UNPACK = $(RM) -R $@ \
	$(foreach f,$(filter %.tar.gz %.tgz,$^), && tar xzf $(f) $(if ${BATCH_MODE},,-v)) \
	$(foreach f,$(filter %.tar.bz2,$^), && tar xjf $(f) $(if ${BATCH_MODE},,-v)) \
	$(foreach f,$(filter %.tar.xz,$^), && tar xJf $(f) $(if ${BATCH_MODE},,-v)) \
	$(foreach f,$(filter %.zip,$^), && unzip $(if ${BATCH_MODE},-q) $(f))

UNPACK_DIR = $(basename $(basename $(notdir $<)))
APPLY = (cd $(UNPACK_DIR) && patch -p1) <
MOVE = mv $(UNPACK_DIR) $@ && touch $@

#
# package rules
#

# yasm

yasm-$(YASM_VERSION).tar.gz:
	$(call download,$(YASM_URL))

yasm: yasm-$(YASM_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.yasm: yasm
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .yasm
CLEAN_PKG += yasm
DISTCLEAN_PKG += yasm-$(YASM_VERSION).tar.gz

# cmake

cmake-$(CMAKE_VERSION).tar.gz:
	$(call download,$(CMAKE_URL))

cmake: cmake-$(CMAKE_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.cmake: cmake
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .cmake
CLEAN_PKG += cmake
DISTCLEAN_PKG += cmake-$(CMAKE_VERSION).tar.gz

# libtool

libtool-$(LIBTOOL_VERSION).tar.xz:
	$(call download,$(LIBTOOL_URL))

libtool: libtool-$(LIBTOOL_VERSION).tar.xz
	$(UNPACK)
	$(MOVE)

.libtool: libtool .automake
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	ln -sf libtool $(PREFIX)/bin/glibtool
	ln -sf libtoolize $(PREFIX)/bin/glibtoolize
	touch $@

CLEAN_PKG += libtool
DISTCLEAN_PKG += libtool-$(LIBTOOL_VERSION).tar.xz
CLEAN_FILE += .libtool

# GNU tar (with xz support)

tar-$(TAR_VERSION).tar.bz2:
	$(call download,$(TAR_URL))

tar: tar-$(TAR_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.tar: tar
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_PKG += tar
DISTCLEAN_PKG += tar-$(TAR_VERSION).tar.bz2
CLEAN_FILE += .tar

# xz

xz-$(XZ_VERSION).tar.bz2:
	$(call download,$(XZ_URL))

xz: xz-$(XZ_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.xz: xz
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_PKG += xz
DISTCLEAN_PKG += xz-$(XZ_VERSION).tar.bz2
CLEAN_FILE += .xz

# autoconf

autoconf-$(AUTOCONF_VERSION).tar.gz:
	$(call download,$(AUTOCONF_URL))

autoconf: autoconf-$(AUTOCONF_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.autoconf: autoconf .pkg-config
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .autoconf
CLEAN_PKG += autoconf
DISTCLEAN_PKG += autoconf-$(AUTOCONF_VERSION).tar.gz

# automake

automake-$(AUTOMAKE_VERSION).tar.gz:
	$(call download,$(AUTOMAKE_URL))

automake: automake-$(AUTOMAKE_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.automake: automake .autoconf
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .automake
CLEAN_PKG += automake
DISTCLEAN_PKG += automake-$(AUTOMAKE_VERSION).tar.gz

# m4

m4-$(M4_VERSION).tar.gz:
	$(call download,$(M4_URL))

m4: m4-$(M4_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.m4: m4
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .m4
CLEAN_PKG += m4
DISTCLEAN_PKG += m4-$(M4_VERSION).tar.gz

# pkg-config

ifdef HAVE_LINUX
.pkg-config:
	touch $@
else
pkg-config-$(PKGCFG_VERSION).tar.gz:
	$(call download,$(PKGCFG_URL))

pkgconfig: pkg-config-$(PKGCFG_VERSION).tar.gz
	$(UNPACK)
	mv pkg-config-lite-$(PKGCFG_VERSION) pkg-config-$(PKGCFG_VERSION)
	$(MOVE)

.pkg-config: pkgconfig
	(cd pkgconfig; ./configure --prefix=$(PREFIX) --disable-shared --enable-static && $(MAKE) && $(MAKE) install)
	touch $@
endif

CLEAN_FILE += .pkg-config
CLEAN_PKG += pkgconfig
DISTCLEAN_PKG += pkg-config-$(PKGCFG_VERSION).tar.gz

# gas-preprocessor
gas-preprocessor-$(GAS_VERSION).pl:
	$(call download,$(GAS_URL))

.gas: gas-preprocessor-$(GAS_VERSION).pl
	mkdir -p $(PREFIX)/bin
	cp gas-preprocessor-$(GAS_VERSION).pl $(PREFIX)/bin/gas-preprocessor.pl
	chmod a+x $(PREFIX)/bin/gas-preprocessor.pl # needs to be executable
	touch $@

CLEAN_FILE += .gas
DISTCLEAN_PKG += gas-preprocessor-$(GAS_VERSION).pl

# GNU sed

sed-$(SED_VERSION).tar.bz2:
	$(call download,$(SED_URL))

sed: sed-$(SED_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.sed: sed
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_PKG += sed
DISTCLEAN_PKG += sed-$(SED_VERSION).tar.bz2
CLEAN_FILE += .sed

# GNU gettext

gettext-$(GETTEXT_VERSION).tar.gz:
	$(call download,$(GETTEXT_URL))

gettext: gettext-$(GETTEXT_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.gettext: gettext
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .gettext
CLEAN_PKG += gettext
DISTCLEAN_PKG += gettext-$(CMAKE_VERSION).tar.gz

#
#
#

clean:
	rm -fr $(CLEAN_FILE) $(CLEAN_PKG) build/

distclean: clean
	rm -fr $(DISTCLEAN_PKG)

.PHONY: all clean distclean
