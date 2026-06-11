# Nettle

#NETTLE_VERSION := 4.0
#NETTLE_URL := https://ftp.gnu.org/gnu/nettle/nettle-$(NETTLE_VERSION).tar.gz

# Use GitHub to give the GNU servers a break
NETTLE_VERSION := nettle_4.0_release_20260205
NETTLE_URL := https://github.com/gnutls/nettle/archive/refs/tags/$(NETTLE_VERSION).tar.gz

PKG_CPE += cpe:2.3:a:nettle_project:nettle:4.0:*:*:*:*:*:*:*
PKGS += nettle

ifeq ($(call need_pkg,"nettle >= 3.6"),)
PKGS_FOUND += nettle
endif
DEPS_nettle = gmp

$(TARBALLS)/nettle-$(NETTLE_VERSION).tar.gz:
	$(call download,$(NETTLE_URL))

.sum-nettle: nettle-$(NETTLE_VERSION).tar.gz

nettle: nettle-$(NETTLE_VERSION).tar.gz .sum-nettle
	$(UNPACK)
#	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.nettle: nettle
ifdef HAVE_IOS
	cd $< && sed -i.orig s/-ggdb3//g configure.ac
endif
	cd $< && autoreconf
	cd $< && $(HOSTVARS) ./configure --disable-documentation $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
