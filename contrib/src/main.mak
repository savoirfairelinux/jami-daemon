# Main makefile for VLC 3rd party libraries ("contrib")
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

all: install

# bootstrap configuration
include config.mak

TOPSRC ?= ../../contrib
TOPDST ?= ..
SRC := $(TOPSRC)/src
TARBALLS := $(TOPSRC)/tarballs

PATH :=$(abspath ../../extras/tools/build/bin):$(PATH)
export PATH

PKGS_ALL := $(patsubst $(SRC)/%/rules.mak,%,$(wildcard $(SRC)/*/rules.mak))
DATE := $(shell date +%Y%m%d)
VPATH := $(TARBALLS)

# Set following to non-empty to remove uneeded progression reports (i.e. with automatics builds)
BATCH_MODE = 1

# Common download locations
GNU := https://ftpmirror.gnu.org
SF := https://sourceforge.net/projects
CONTRIB_VIDEOLAN ?= https://downloads.videolan.org/pub/contrib

#
# Machine-dependent variables
#

PREFIX ?= $(TOPDST)/$(HOST)
PREFIX := $(abspath $(PREFIX))
ifneq ($(HOST),$(BUILD))
HAVE_CROSS_COMPILE = 1
endif
ARCH := $(shell $(SRC)/get-arch.sh $(HOST))

ifeq ($(ARCH)-$(HAVE_WIN32),x86_64-1)
HAVE_WIN64 := 1
endif

ifdef HAVE_CROSS_COMPILE
ifdef HAVE_WIN32
PKG_CONFIG ?= $(CROSS_COMPILE)pkg-config --static
else
PKG_CONFIG ?= pkg-config --static
endif
PKG_CONFIG_PATH_CUSTOM = $(PREFIX)/lib/pkgconfig
export PKG_CONFIG_PATH_CUSTOM
else
PKG_CONFIG ?= pkg-config
endif

PKG_CONFIG_PATH := $(PREFIX)/lib/pkgconfig:$(PREFIX)/lib/$(HOST)/pkgconfig:$(PKG_CONFIG_PATH)
export PKG_CONFIG_PATH

ifdef HAVE_CROSS_COMPILE
need_pkg = 1
else
need_pkg = $(shell $(PKG_CONFIG) $(1) || echo 1)
endif
#
# Default values for tools
#
ifndef HAVE_CROSS_COMPILE
ifneq ($(findstring $(origin CC),undefined default),)
CC := gcc
endif
ifneq ($(findstring $(origin CXX),undefined default),)
CXX := g++
endif
ifneq ($(findstring $(origin LD),undefined default),)
LD := ld
endif
ifneq ($(findstring $(origin AR),undefined default),)
AR := ar
endif
ifneq ($(findstring $(origin RANLIB),undefined default),)
RANLIB := ranlib
endif
ifneq ($(findstring $(origin STRIP),undefined default),)
STRIP := strip
endif
else
ifneq ($(findstring $(origin CC),undefined default),)
CC := $(CROSS_COMPILE)gcc
endif
ifneq ($(findstring $(origin CXX),undefined default),)
CXX := $(CROSS_COMPILE)g++
endif
ifneq ($(findstring $(origin LD),undefined default),)
LD := $(CROSS_COMPILE)ld
endif
ifneq ($(findstring $(origin AR),undefined default),)
AR := $(CROSS_COMPILE)ar
endif
ifneq ($(findstring $(origin RANLIB),undefined default),)
RANLIB := $(CROSS_COMPILE)ranlib
endif
ifneq ($(findstring $(origin STRIP),undefined default),)
STRIP := $(CROSS_COMPILE)strip
endif
endif

ifdef HAVE_ANDROID
CC := $(CROSS_COMPILE)clang
CXX := $(CROSS_COMPILE)clang++
endif

ifdef HAVE_MACOSX
MIN_OSX_VERSION=10.8
CC=xcrun cc
CXX=xcrun c++
AR=xcrun ar
LD=xcrun ld
STRIP=xcrun strip
RANLIB=xcrun ranlib
EXTRA_COMMON := -isysroot $(MACOSX_SDK) -mmacosx-version-min=$(MIN_OSX_VERSION) -DMACOSX_DEPLOYMENT_TARGET=$(MIN_OSX_VERSION)
EXTRA_CXXFLAGS += -stdlib=libc++
EXTRA_LDFLAGS += -Wl,-syslibroot,$(MACOSX_SDK)
ifeq ($(ARCH),x86_64)
EXTRA_COMMON += -m64
else
EXTRA_COMMON += -m32
endif

XCODE_FLAGS = -sdk macosx$(OSX_VERSION)
ifeq ($(shell xcodebuild -version 2>/dev/null | tee /dev/null|head -1|cut -d\  -f2|cut -d. -f1),3)
XCODE_FLAGS += ARCHS=$(ARCH)
# XCode 3 doesn't support -arch
else
XCODE_FLAGS += -arch $(ARCH)
endif

endif

CCAS=$(CC) -c

ifdef HAVE_IOS

ifndef MIN_IOS_VERSION
MIN_IOS_VERSION=9.3
endif

CC=xcrun clang
CXX=xcrun clang++
CCAS=$(CC) -c
AR=xcrun ar
LD=xcrun ld
STRIP=xcrun strip
RANLIB=xcrun ranlib

EXTRA_CFLAGS=-arch $(ARCH) -isysroot $(IOS_SDK)
ifeq ($(IOS_TARGET_PLATFORM),iPhoneOS)
EXTRA_CFLAGS += -miphoneos-version-min=$(MIN_IOS_VERSION) -fembed-bitcode
else
EXTRA_CFLAGS += -mios-simulator-version-min=$(MIN_IOS_VERSION)
endif

EXTRA_CXXFLAGS=$(EXTRA_CFLAGS) -stdlib=libc++
EXTRA_LDFLAGS=$(EXTRA_CFLAGS)

endif

ifdef HAVE_WIN32
ifneq ($(shell $(CC) $(CFLAGS) -E -dM -include _mingw.h - < /dev/null | grep -E __MINGW64_VERSION_MAJOR),)
HAVE_MINGW_W64 := 1
endif
endif

ifdef HAVE_SOLARIS
ifeq ($(ARCH),x86_64)
EXTRA_COMMON += -m64
else
EXTRA_COMMON += -m32
endif
endif

EXTRA_CXXFLAGS += -std=c++14

ifdef ENABLE_DEBUG
EXTRA_CFLAGS += -g -fno-omit-frame-pointer
else
EXTRA_CFLAGS += -DNDEBUG=1
endif

cppcheck = $(shell $(CC) $(CFLAGS) -E -dM - < /dev/null | grep -E $(1))

EXTRA_CPPFLAGS += -I$(PREFIX)/include
EXTRA_LDFLAGS += -L$(PREFIX)/lib

CPPFLAGS := $(CPPFLAGS) $(EXTRA_CPPFLAGS)
CFLAGS := $(CFLAGS) $(EXTRA_CPPFLAGS) $(EXTRA_COMMON) $(EXTRA_CFLAGS)
CXXFLAGS := $(CXXFLAGS) $(EXTRA_CPPFLAGS) $(EXTRA_COMMON) $(EXTRA_CXXFLAGS)
LDFLAGS := $(LDFLAGS) $(EXTRA_LDFLAGS)
# Do not export those! Use HOSTVARS.

# Do the FPU detection, after we have figured out our compilers and flags.
ifneq ($(findstring $(ARCH),aarch64 i386 ppc ppc64 sparc sparc64 x86_64),)
# This should be consistent with include/vlc_cpu.h
HAVE_FPU = 1
else ifneq ($(findstring $(ARCH),arm),)
ifneq ($(call cppcheck, __VFP_FP__)),)
ifeq ($(call cppcheck, __SOFTFP__),)
HAVE_FPU = 1
endif
endif
else ifneq ($(call cppcheck, __mips_hard_float),)
HAVE_FPU = 1
endif

ACLOCAL_AMFLAGS += -I$(PREFIX)/share/aclocal
export ACLOCAL_AMFLAGS



ifndef GIT
ifeq ($(shell git --version >/dev/null 2>&1 || echo FAIL),)
GIT = git
endif
endif
GIT ?= $(error git not found!)

ifndef SVN
ifeq ($(shell svn --version >/dev/null 2>&1 || echo FAIL),)
SVN = svn
endif
endif
SVN ?= $(error subversion client (svn) not found!)

ifeq ($(DISABLE_CONTRIB_DOWNLOADS),TRUE)
download = $(error Trying to download $(1) but DISABLE_CONTRIB_DOWNLOADS is TRUE, aborting.)
else ifeq ($(shell curl --version >/dev/null 2>&1 || echo FAIL),)
download = curl $(if ${BATCH_MODE},-sS) -f -L --retry-delay 10 --retry 4 -- "$(1)" > "$@"
else ifeq ($(shell wget --version >/dev/null 2>&1 || echo FAIL),)
download = rm -f $@.tmp && \
	wget $(if ${BATCH_MODE},-nv) --passive -t 4 -w 10 -c -p -O $@.tmp "$(1)" && \
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

ifeq ($(shell which bzcat >/dev/null 2>&1 || echo FAIL),)
BZCAT = bzcat
else
BZCAT ?= $(error Bunzip2 client (bzcat) not found!)
endif

ifeq ($(shell gzcat --version >/dev/null 2>&1 || echo FAIL),)
ZCAT = gzcat
else ifeq ($(shell zcat --version >/dev/null 2>&1 || echo FAIL),)
ZCAT = zcat
else
ZCAT ?= $(error Gunzip client (zcat) not found!)
endif

ifeq ($(shell sha512sum --version >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = sha512sum --check
else ifeq ($(shell shasum --version >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = shasum -a 512 --check
else ifeq ($(shell openssl version >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = openssl dgst -sha512
else
SHA512SUM = $(error SHA-512 checksumming not found!)
endif

#
# Common helpers
#
HOSTCONF := --prefix="$(PREFIX)"
HOSTCONF += --datarootdir="$(PREFIX)/share"
HOSTCONF += --includedir="$(PREFIX)/include"
HOSTCONF += --libdir="$(PREFIX)/lib"
HOSTCONF += --build="$(BUILD)" --host="$(HOST)" --target="$(HOST)"
HOSTCONF += --program-prefix=""
# libtool stuff:
HOSTCONF += --disable-dependency-tracking

ifdef HAVE_LINUX
HOSTCONF += --enable-static --disable-shared
endif

ifdef HAVE_IOS
HOSTCONF += --enable-static --disable-shared
endif

ifdef HAVE_WIN32
HOSTCONF += --enable-static --disable-shared --without-pic
PIC :=
else
HOSTCONF += --with-pic
PIC := -fPIC
endif

HOSTTOOLS := \
	CC="$(CC)" CXX="$(CXX)" LD="$(LD)" \
	AR="$(AR)" CCAS="$(CCAS)" RANLIB="$(RANLIB)" STRIP="$(STRIP)" \
	PATH="$(PREFIX)/bin:$(PATH)"
# this part is different from VideoLan main.mak
HOSTVARS_NOPIC := $(HOSTTOOLS) \
	CPPFLAGS="$(CPPFLAGS)" \
	CFLAGS="$(CFLAGS)" \
	CXXFLAGS="$(CXXFLAGS)" \
	LDFLAGS="$(LDFLAGS)"
HOSTVARS := $(HOSTTOOLS) \
	CPPFLAGS="$(CPPFLAGS) $(PIC)" \
	CFLAGS="$(CFLAGS) $(PIC)" \
	CXXFLAGS="$(CXXFLAGS) $(PIC)" \
	LDFLAGS="$(LDFLAGS)"

ifeq ($(DISABLE_CONTRIB_DOWNLOADS),TRUE)
download_git = $(error Trying to clone $(1) but DISABLE_CONTRIB_DOWNLOADS is TRUE, aborting.)
else
download_git = \
	rm -Rf $(@:.tar.xz=) && \
	$(GIT) clone $(2:%=--branch %) $(1) $(@:.tar.xz=) && \
	(cd $(@:.tar.xz=) && $(GIT) checkout $(3:%= %)) && \
	rm -Rf $(@:%.tar.xz=%)/.git && \
	(cd $(dir $@) && \
	tar cvJ $(notdir $(@:.tar.xz=))) > $@ && \
	rm -Rf $(@:.tar.xz=)
endif

checksum = \
	$(foreach f,$(filter $(TARBALLS)/%,$^), \
		grep -- " $(f:$(TARBALLS)/%=%)$$" \
			"$(SRC)/$(patsubst .sum-%,%,$@)/$(2)SUMS" |) \
	(cd $(TARBALLS) && $(1))
ifeq ($(DISABLE_CONTRIB_CHECKSUMS),TRUE)
    CHECK_SHA512 = @echo "Skipping checksum verification..."
else
    CHECK_SHA512 = $(call checksum,$(SHA512SUM),SHA512)
endif
UNPACK = $(RM) -R $@ \
	$(foreach f,$(filter %.tar.gz %.tgz,$^), && tar xzf $(f) $(if ${BATCH_MODE},,-v)) \
	$(foreach f,$(filter %.tar.bz2,$^), && tar xjf $(f) $(if ${BATCH_MODE},,-v)) \
	$(foreach f,$(filter %.tar.xz,$^), && tar xJf $(f) $(if ${BATCH_MODE},,-v)) \
	$(foreach f,$(filter %.zip,$^), && unzip $(if ${BATCH_MODE},-q) $(f))
UNPACK_DIR = $(basename $(basename $(notdir $<)))
APPLY = (cd $(UNPACK_DIR) && patch -fp1) <
APPLY_BIN = (cd $(UNPACK_DIR) && patch --binary -flp1) <
pkg_static = (cd $(UNPACK_DIR) && ../../../contrib/src/pkg-static.sh $(1))
MOVE = mv $(UNPACK_DIR) $@ && touch $@

AUTOMAKE_DATA_DIRS=$(foreach n,$(foreach n,$(subst :, ,$(shell echo $$PATH)),$(abspath $(n)/../share)),$(wildcard $(n)/automake*))
UPDATE_AUTOCONFIG = for dir in $(AUTOMAKE_DATA_DIRS); do \
		if test -f "$${dir}/config.sub" -a -f "$${dir}/config.guess"; then \
			cp "$${dir}/config.sub" "$${dir}/config.guess" $(UNPACK_DIR); \
			break; \
		fi; \
	done

RECONF = mkdir -p -- $(PREFIX)/share/aclocal && \
	cd $< && autoreconf -fiv $(ACLOCAL_AMFLAGS)
CMAKE = cmake . -DCMAKE_TOOLCHAIN_FILE=$(abspath toolchain.cmake) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX)

#
# Per-package build rules
#
include $(SRC)/*/rules.mak

ifeq ($(PKGS_DISABLE), all)
PKGS :=
endif
#
# Targets
#
ifneq ($(filter $(PKGS_DISABLE),$(PKGS_ENABLE)),)
$(error Same package(s) disabled and enabled at the same time)
endif
# Apply automatic selection (= remove distro packages):
PKGS_AUTOMATIC := $(filter-out $(PKGS_FOUND),$(PKGS))
# Apply manual selection (from bootstrap):
PKGS_MANUAL := $(sort $(PKGS_ENABLE) $(filter-out $(PKGS_DISABLE),$(PKGS_AUTOMATIC)))
# Resolve dependencies:
dep_on = $(if $(filter $1,$2),\
  $(error Dependency cycle detected: $(patsubst %,% ->,$2) $(filter $1,$2)),\
  $(sort $(foreach p,$(filter-out $(PKGS_FOUND),$(1)),$(p) $(call dep_on,$(DEPS_$(p)),$2 $(p)))))
PKGS_DEPS := $(call dep_on,$(PKGS_MANUAL))
PKGS := $(sort $(PKGS_MANUAL) $(PKGS_DEPS))

convert-static:
	for p in $(PREFIX)/lib/pkgconfig/*.pc; do $(SRC)/pkg-static.sh $$p; done
fetch: $(PKGS:%=.sum-%)
fetch-all: $(PKGS_ALL:%=.sum-%)
install: $(PKGS:%=.%) convert-static

mostlyclean:
	-$(RM) $(foreach p,$(PKGS_ALL),.$(p) .sum-$(p) .dep-$(p))
	-$(RM) toolchain.cmake
	-$(RM) -R "$(PREFIX)"
	-$(RM) -R */

clean: mostlyclean
	-$(RM) $(TARBALLS)/*.*

distclean: clean
	$(RM) config.mak
	unlink Makefile

# TODO: set up the correct url
#PREBUILT_URL=$(URL)/contrib/$(HOST)/ring-contrib-$(HOST)-latest.tar.bz2

ring-contrib-$(HOST)-latest.tar.bz2:
	$(call download,$(PREBUILT_URL))

prebuilt: ring-contrib-$(HOST)-latest.tar.bz2
	-$(UNPACK)
	mv $(HOST) $(TOPDST)
	cd $(TOPDST)/$(HOST) && $(SRC)/change_prefix.sh

package: install
	rm -Rf tmp/
	mkdir -p tmp/
	cp -r $(PREFIX) tmp/
	# remove useless files
	cd tmp/$(notdir $(PREFIX)); \
		cd share; rm -Rf man doc gtk-doc info lua projectM gettext; cd ..; \
		rm -Rf man sbin etc lib/lua lib/sidplay
	cd tmp/$(notdir $(PREFIX)) && $(abspath $(SRC))/change_prefix.sh $(PREFIX) @@CONTRIB_PREFIX@@
	(cd tmp && tar c $(notdir $(PREFIX))/) | bzip2 -c > ../ring-contrib-$(HOST)-$(DATE).tar.bz2

list:
	@echo All packages:
	@echo '  $(PKGS_ALL)' | fmt
	@echo Distribution-provided packages:
	@echo '  $(PKGS_FOUND)' | fmt
	@echo Automatically selected packages:
	@echo '  $(PKGS_AUTOMATIC)' | fmt
	@echo Manually deselected packages:
	@echo '  $(PKGS_DISABLE)' | fmt
	@echo Manually selected packages:
	@echo '  $(PKGS_ENABLE)' | fmt
	@echo Depended-on packages:
	@echo '  $(PKGS_DEPS)' | fmt
	@echo To-be-built packages:
	@echo '  $(PKGS)' | fmt

.PHONY: all fetch fetch-all install mostlyclean clean distclean package list prebuilt

# CMake toolchain
toolchain.cmake:
	$(RM) $@
ifdef HAVE_WIN32
	echo "set(CMAKE_SYSTEM_NAME Windows)" >> $@
	echo "set(CMAKE_RC_COMPILER $(HOST)-windres)" >> $@
endif
ifdef HAVE_DARWIN_OS
	echo "set(CMAKE_SYSTEM_NAME Darwin)" >> $@
	echo "set(CMAKE_C_FLAGS $(CFLAGS))" >> $@
	echo "set(CMAKE_CXX_FLAGS $(CFLAGS))" >> $@
	echo "set(CMAKE_LD_FLAGS $(LDFLAGS))" >> $@
	echo "set(CMAKE_AR ar CACHE FILEPATH "Archiver")" >> $@
ifdef HAVE_IOS
	echo "set(CMAKE_OSX_SYSROOT $(IOS_SDK))" >> $@
else
	echo "set(CMAKE_OSX_SYSROOT $(MACOSX_SDK))" >> $@
endif
endif
ifdef HAVE_CROSS_COMPILE
	echo "set(_CMAKE_TOOLCHAIN_PREFIX $(CROSS_COMPILE))" >> $@
ifdef HAVE_ANDROID
	echo "set(CMAKE_SYSTEM_NAME Android)" >> $@
	echo "set(CMAKE_ANDROID_STANDALONE_TOOLCHAIN \"$(ANDROID_TOOLCHAIN)\")" >> $@
	echo "set(CMAKE_ANDROID_ARCH_ABI $(ANDROID_ABI))" >> $@
	echo "set(CMAKE_SYSTEM_VERSION $(subst android-,,$(ANDROID_API)))" >> $@
	echo "set(CMAKE_ANDROID_STL_TYPE c++_static)" >> $@
endif
endif
	echo "set(CMAKE_C_COMPILER $(CC))" >> $@
	echo "set(CMAKE_CXX_COMPILER $(CXX))" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH $(PREFIX))" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@

# Default pattern rules
.sum-%: $(SRC)/%/SHA512SUMS
	$(CHECK_SHA512)
	touch $@

.sum-%:
	$(error Download and check '$@' target not defined for $* contrib)

# Dummy dependency on found packages
$(patsubst %,.dep-%,$(PKGS_FOUND)): .dep-%:
	touch $@

# Real dependency on missing packages
$(patsubst %,.dep-%,$(filter-out $(PKGS_FOUND),$(PKGS_ALL))): .dep-%: .%
	touch -r $< $@

# dump list of packages to build
list-build-packages:
	@echo ${PKGS}

.SECONDEXPANSION:

# Dependency propagation (convert 'DEPS_foo = bar' to '.foo: .bar')
$(foreach p,$(PKGS_ALL),.$(p)): .%: $$(foreach d,$$(DEPS_$$*),.dep-$$(d))

.DELETE_ON_ERROR:

# Disable -j option for the top Makefile as this framework doesn't support well
# this and some contrib may be not well build or even not build at all.
# Notice that projects' rules.mak file use parallel jobs even with this.
.NOTPARALLEL:
