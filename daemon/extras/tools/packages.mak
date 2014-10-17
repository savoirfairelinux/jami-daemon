GNU=http://ftp.gnu.org/gnu
APACHE=http://mir2.ovh.net/ftp.apache.org/dist
SF= http://downloads.sourceforge.net/project

YASM_VERSION=1.2.0
#YASM_URL=$(CONTRIB_VIDEOLAN)/yasm-$(YASM_VERSION).tar.gz
YASM_URL=http://www.tortall.net/projects/yasm/releases/yasm-$(YASM_VERSION).tar.gz

CMAKE_VERSION=2.8.12.2
CMAKE_URL=http://www.cmake.org/files/v2.8/cmake-$(CMAKE_VERSION).tar.gz

LIBTOOL_VERSION=2.4.2
LIBTOOL_URL=$(GNU)/libtool/libtool-$(LIBTOOL_VERSION).tar.gz

AUTOCONF_VERSION=2.69
AUTOCONF_URL=$(GNU)/autoconf/autoconf-$(AUTOCONF_VERSION).tar.gz

AUTOMAKE_VERSION=1.14
AUTOMAKE_URL=$(GNU)/automake/automake-$(AUTOMAKE_VERSION).tar.gz

M4_VERSION=1.4.16
M4_URL=$(GNU)/m4/m4-$(M4_VERSION).tar.gz

PKGCFG_VERSION=0.28-1
#PKGCFG_URL=http://downloads.videolan.org/pub/videolan/testing/contrib/pkg-config-$(PKGCFG_VERSION).tar.gz
PKGCFG_URL=$(SF)/pkgconfiglite/$(PKGCFG_VERSION)/pkg-config-lite-$(PKGCFG_VERSION).tar.gz

TAR_VERSION=1.26
TAR_URL=$(GNU)/tar/tar-$(TAR_VERSION).tar.bz2

XZ_VERSION=5.0.3
XZ_URL=http://tukaani.org/xz/xz-$(XZ_VERSION).tar.bz2

GAS_VERSION=72887b9
GAS_URL=http://git.libav.org/?p=gas-preprocessor.git;a=snapshot;h=$(GAS_VERSION);sf=tgz

SED_VERSION=4.2.2
SED_URL=$(GNU)/sed/sed-$(SED_VERSION).tar.bz2

ANT_VERSION=1.9.4
ANT_URL=$(APACHE)/ant/binaries/apache-ant-$(ANT_VERSION)-bin.tar.bz2
