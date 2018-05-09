GNU=https://ftp.gnu.org/gnu
APACHE=http://mir2.ovh.net/ftp.apache.org/dist
SF= https://downloads.sourceforge.net/project

YASM_VERSION=1.2.0
#YASM_URL=$(CONTRIB_VIDEOLAN)/yasm-$(YASM_VERSION).tar.gz
YASM_URL=https://www.tortall.net/projects/yasm/releases/yasm-$(YASM_VERSION).tar.gz

CMAKE_VERSION=3.2.2
CMAKE_URL=https://www.cmake.org/files/v3.2/cmake-$(CMAKE_VERSION).tar.gz

LIBTOOL_VERSION=2.4.6
LIBTOOL_URL=$(GNU)/libtool/libtool-$(LIBTOOL_VERSION).tar.xz

AUTOCONF_VERSION=2.69
AUTOCONF_URL=$(GNU)/autoconf/autoconf-$(AUTOCONF_VERSION).tar.gz

AUTOMAKE_VERSION=1.15.1
AUTOMAKE_URL=$(GNU)/automake/automake-$(AUTOMAKE_VERSION).tar.gz

M4_VERSION=1.4.18
M4_URL=$(GNU)/m4/m4-$(M4_VERSION).tar.gz

PKGCFG_VERSION=0.28-1
#PKGCFG_URL=http://downloads.videolan.org/pub/videolan/testing/contrib/pkg-config-$(PKGCFG_VERSION).tar.gz
PKGCFG_URL=$(SF)/pkgconfiglite/$(PKGCFG_VERSION)/pkg-config-lite-$(PKGCFG_VERSION).tar.gz

TAR_VERSION=1.28
TAR_URL=$(GNU)/tar/tar-$(TAR_VERSION).tar.bz2

XZ_VERSION=5.2.2
XZ_URL=https://tukaani.org/xz/xz-$(XZ_VERSION).tar.bz2

GAS_VERSION=cbe88474ec196370161032a3863ec65050f70ba4
GAS_URL=https://raw.githubusercontent.com/FFmpeg/gas-preprocessor/$(GAS_VERSION)/gas-preprocessor.pl

SED_VERSION=4.2.2
SED_URL=$(GNU)/sed/sed-$(SED_VERSION).tar.bz2

GETTEXT_VERSION=0.19.6
GETTEXT_URL=$(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.gz
